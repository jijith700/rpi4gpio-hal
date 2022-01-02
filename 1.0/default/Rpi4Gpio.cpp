// FIXME: your file license if you have one
#define LOG_TAG "android.hardware.rpi4gpio@1.0-impl"

#include <log/log.h>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/signalfd.h>
#include <sys/timerfd.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <cutils/uevent.h>
#include <utils/Errors.h>
#include <utils/StrongPointer.h>
#include <cstring>
#include <fcntl.h>
#include <cassert>
#include <dirent.h>
#include <iostream>
#include <fstream>
#include <pthread.h>
#include <cstdio>
#include <unistd.h>

#include "Rpi4Gpio.h"

// Define defaults that work on the Raspberry pi 3B+.
const int kDefaultGpioPortOffset = 0;


// GPIO sysfs path
const char *const kGPIOSysfsPath = "/sys/class/gpio";
const int kPinLed = 23;  // Number(gpio17)  = kDefaultGpioPortOffset + kPinLed = 458 + 17 = 475
const int kPinSW = 24;   // Number(gpio18)  = kDefaultGpioPortOffset + kPinSW = 458 + 18 = 476

namespace android::hardware::rpi4gpio::implementation {

    // Set by the signal handler to destroy the thread
    volatile bool destroyThread;

    // Added manually
    Rpi4Gpio::Rpi4Gpio() {

        std::ostringstream gpio_export_path, gpio_led_port, gpio_sw_port;
        std::ostringstream gpio_led_direction;
        std::ostringstream gpio_sw_direction;
        std::ostringstream gpio_sw_edge;
        int hGpioexport = -1;
        int hGpioLedDir = -1;
        int hGpioSWDir = -1;
        int hGpioSWEdge = -1;
        int ret = -1;

        valid = 1;
        state = LedStatus::LED_OFF;

        // Exit by any error
        do {

            //gpio_export_path = base::StringPrintf("%s/export", kGPIOSysfsPath);
            gpio_export_path << kGPIOSysfsPath << "/export";
            hGpioexport = open(gpio_export_path.str().c_str(), O_WRONLY);
            if (hGpioexport < 0) {
                ALOGE("Rpi3gpio Export Init Failed (%s)", gpio_export_path.str().c_str());
                valid = -1;
                break;
            }

            ALOGI("Rpi3gpio Export Init Successfully (%s)", gpio_export_path.str().c_str());

            // Add export for LED
            // Number(gpio17)  = kDefaultGpioPortOffset + kPinLed = 458 + 17 = 475
            // gpio_led_port = base::StringPrintf("%d", kDefaultGpioPortOffset + kPinLed);
            gpio_led_port << (kDefaultGpioPortOffset + kPinLed);
            ret = write(hGpioexport, gpio_led_port.str().c_str(), gpio_led_port.str().size());
            if (ret < 0) {
                ALOGE("Rpi3gpio Led Port Init Failed");
                valid = -1;
                break;
            }

            ALOGI("Rpi3gpio Led Port Init Successfully");
            // Define the direction(OUT) of LED port
            // gpio_led_direction = base::StringPrintf("%s/gpio%d/direction", kGPIOSysfsPath, kDefaultGpioPortOffset + kPinLed);
            gpio_led_direction << kGPIOSysfsPath << "/gpio" << (kDefaultGpioPortOffset + kPinLed)
                               << "/direction";
            hGpioLedDir = open(gpio_led_direction.str().c_str(), O_WRONLY);
            if (hGpioLedDir < 0) {
                ALOGE("Rpi3gpio Led direction Init Failed (%s)", gpio_led_direction.str().c_str());
                valid = -1;
                break;
            }
            ALOGI("Rpi3gpio Led direction Init Successfully (%s)",
                  gpio_led_direction.str().c_str());

            ret = write(hGpioLedDir, "out", strlen("out"));
            if (ret < 0) {
                ALOGE("Rpi3gpio Led Port direction Init Failed");
                valid = -1;
                break;
            }
            ALOGI("Rpi3gpio Led Port direction Init Successfully");

            // Add export for SW
            // Number(gpio18)  = kDefaultGpioPortOffset + kPinSW = 458 + 18 = 476
            // gpio_sw_port = base::StringPrintf("%d", kDefaultGpioPortOffset + kPinSW);
            gpio_sw_port << (kDefaultGpioPortOffset + kPinSW);
            ret = write(hGpioexport, gpio_sw_port.str().c_str(), gpio_sw_port.str().size());
            if (ret < 0) {
                ALOGE("Rpi3gpio SW Port Init Failed");
                valid = -1;
                break;
            }
            ALOGI("Rpi3gpio SW Port Init Successfully");

            // Define the direction(IN) of SW port
            // gpio_sw_direction = base::StringPrintf("%s/gpio%d/direction", kGPIOSysfsPath, kDefaultGpioPortOffset + kPinSW);
            gpio_sw_direction << kGPIOSysfsPath << "/gpio" << (kDefaultGpioPortOffset + kPinSW)
                              << "/direction";
            hGpioSWDir = open(gpio_sw_direction.str().c_str(), O_WRONLY);
            if (hGpioSWDir < 0) {
                ALOGE("Rpi3gpio SW direction Init Failed");
                valid = -1;
                break;
            }
            ALOGI("Rpi3gpio SW direction Init Successfully");

            ret = write(hGpioSWDir, "in", strlen("in"));
            if (ret < 0) {
                ALOGE("Rpi3gpio SW Port direction Init Failed");
                valid = -1;
                break;
            }
            ALOGI("Rpi3gpio SW Port direction Init Successfully");

            // Define the edge(rising) of SW port
            gpio_sw_edge << kGPIOSysfsPath << "/gpio" << (kDefaultGpioPortOffset + kPinSW)
                         << "/edge";
            hGpioSWEdge = open(gpio_sw_edge.str().c_str(), O_WRONLY);
            if (hGpioSWEdge < 0) {
                ALOGE("%s Init Failed", gpio_sw_edge.str().c_str());
                valid = -1;
                break;
            }
            ALOGI("%s Init Successfully", gpio_sw_edge.str().c_str());

            ret = write(hGpioSWEdge, "rising", strlen("rising"));
            if (ret < 0) {
                ALOGE("Rpi3gpio SW Edge rising Init Failed");
                valid = -1;
                break;
            }
            ALOGI("Rpi3gpio SW Edge rising Init Successfully");

        } while (0);

        // Close all the local file handle
        // hGpioexport, hGpioLedDir, hGpioSWDir;

        if (hGpioexport >= 0) {
            close(hGpioexport);
        }

        if (hGpioLedDir >= 0) {
            close(hGpioLedDir);
        }

        if (hGpioSWDir >= 0) {
            close(hGpioSWDir);
        }

        if (hGpioSWEdge >= 0) {
            close(hGpioSWEdge);
        }

        if (valid >= 0) {
            ALOGI("Rpi3gpio Init Successfully, valid:%d", valid);
        } else {
            ALOGE("Rpi3gpio Init Failed, valid:%d", valid);
        }

    }

    Rpi4Gpio::~Rpi4Gpio() {
        // Release gpio
        // $ echo 475 > /sys/class/gpio/unexport
        // $ echo 476 > /sys/class/gpio/unexport
        std::ostringstream gpio_unexport_path, gpio_led_port, gpio_sw_port;
        int hGpiounexport = -1;
        int hGpioLedDir = -1;
        int hGpioSWDir = -1;
        int ret = -1;

        do {
            if (1 == valid) {
                gpio_unexport_path << kGPIOSysfsPath << "/unexport";
                hGpiounexport = open(gpio_unexport_path.str().c_str(), O_WRONLY);
                if (hGpiounexport < 0) {
                    ALOGE("Rpi3gpio Unexport Init Failed (%s)", gpio_unexport_path.str().c_str());
                    valid = -1;
                    break;
                }

                ALOGI("Rpi3gpio Unexport Init Successfully (%s)", gpio_unexport_path.str().c_str());

                // Unexport for LED
                // Number(gpio17)  = kDefaultGpioPortOffset + kPinLed = 458 + 17 = 475
                gpio_led_port << (kDefaultGpioPortOffset + kPinLed);
                ret = write(hGpiounexport, gpio_led_port.str().c_str(), gpio_led_port.str().size());
                if (ret < 0) {
                    ALOGE("Rpi3gpio Led Port Unexport Failed");
                    valid = -1;
                    break;
                }

                ALOGI("Rpi3gpio Led Port Unexport Successfully");

                // Unexport for SW
                // Number(gpio18)  = kDefaultGpioPortOffset + kPinSW = 458 + 18 = 476
                gpio_sw_port << (kDefaultGpioPortOffset + kPinSW);
                ret = write(hGpiounexport, gpio_sw_port.str().c_str(), gpio_sw_port.str().size());
                if (ret < 0) {
                    ALOGE("Rpi3gpio SW Port Unexport Failed");
                    valid = -1;
                    break;
                }
                ALOGI("Rpi3gpio SW Port Unexport Successfully");
            }
        } while (0);

        // Close all the local file handle
        // hGpioexport, hGpioLedDir, hGpioSWDir;

        if (hGpiounexport >= 0) {
            close(hGpiounexport);
        }

        if (hGpioLedDir >= 0) {
            close(hGpioLedDir);
        }

        if (hGpioSWDir >= 0) {
            close(hGpioSWDir);
        }
    }


    // Methods from ::android::hardware::rpi4gpio::V1_0::IRpi4Gpio follow.
    Return<LedStatus> Rpi4Gpio::get() {
        // TODO implement
        pthread_mutex_lock(&mutexSW); //mutex lock

        pthread_cond_wait(&conditionSW, &mutexSW); //wait for the SW(gpio 18) pressed

        if (LedStatus::LED_ON == state) {
            state = LedStatus::LED_OFF;
        } else {
            state = LedStatus::LED_ON;
        }

        pthread_mutex_unlock(&mutexSW);
        return state;
        //return LedStatus {};
    }

    Return<int32_t> Rpi4Gpio::set(LedStatus val) {
        if (val == LedStatus::LED_OFF ||
            val == LedStatus::LED_ON)
            state = val;
        else
            return -1;
        return 0;
    }

    Return<void> Rpi4Gpio::on() {
        int hGpioLed = -1;  // the handle of LED port
        int ret = -1;
        std::ostringstream gpio_led_value;

        if (valid < 0) {
            ALOGE("Rpi3gpio is unavailable!");
            return Void();
        }

        // gpio_led_value = base::StringPrintf("%s/gpio%d/value", kGPIOSysfsPath, kDefaultGpioPortOffset + kPinLed);
        gpio_led_value << kGPIOSysfsPath << "/gpio" << (kDefaultGpioPortOffset + kPinLed)
                       << "/value";
        hGpioLed = open(gpio_led_value.str().c_str(), O_WRONLY);
        if (hGpioLed < 0) {
            ALOGE("Rpi3gpio %s is unavailable!", gpio_led_value.str().c_str());
            return Void();
        }


        ret = write(hGpioLed, "1", strlen("1"));
        if (ret < 0) {
            ALOGE("Rpi3gpio LED Port write on Failed");
            //state = LedStatus::LED_BAD_VALUE;
        }

        close(hGpioLed);

        //state = LedStatus::LED_ON;
        ALOGI("Rpi3gpio On, status:%d", state);
        return Void();
    }

    Return<void> Rpi4Gpio::off() {
        int hGpioLed = -1;  // the handle of LED port
        int ret = -1;
        std::ostringstream gpio_led_value;

        if (valid < 0) {
            ALOGE("Rpi3gpio is unavailable! valid=%d", valid);
            return Void();
        }

        // gpio_led_value = base::StringPrintf("%s/gpio%d/value", kGPIOSysfsPath, kDefaultGpioPortOffset + kPinLed);
        gpio_led_value << kGPIOSysfsPath << "/gpio" << (kDefaultGpioPortOffset + kPinLed)
                       << "/value";
        hGpioLed = open(gpio_led_value.str().c_str(), O_WRONLY);
        if (hGpioLed < 0) {
            ALOGE("Rpi3gpio %s is unavailable!", gpio_led_value.str().c_str());
            return Void();
        }


        ret = write(hGpioLed, "0", strlen("0"));
        if (ret < 0) {
            ALOGE("Rpi3gpio LED Port write off Failed");
            //state = LedStatus::LED_BAD_VALUE;
        }

        close(hGpioLed);

        //state = LedStatus::LED_OFF;
        ALOGI("Rpi3gpio Off, status:%d", state);
        return Void();
    }


// Methods from ::android::hidl::base::V1_0::IBase follow.

//IRpi4Gpio* HIDL_FETCH_IRpi4Gpio(const char* /* name */) {
    //return new Rpi4Gpio();
//}
//
}  // namespace android::hardware::rpi4gpio::implementation
