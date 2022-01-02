#define LOG_TAG "android.hardware.rpi4gpio@1.0-service"

#include <android/hardware/rpi4gpio/1.0/IRpi4Gpio.h>
#include <hidl/LegacySupport.h>
#include <log/log.h>
#include <hidl/HidlSupport.h>
#include <hidl/HidlTransportSupport.h>

#include "Rpi4Gpio.h"

using android::hardware::rpi4gpio::V1_0::IRpi4Gpio;
using android::hardware::rpi4gpio::implementation::Rpi4Gpio;
using android::hardware::defaultPassthroughServiceImplementation;
using android::hardware::configureRpcThreadpool;
using android::hardware::joinRpcThreadpool;
using android::sp;


int main() {
// Binder approach
    sp<IRpi4Gpio> service = new Rpi4Gpio();
    configureRpcThreadpool(3, true /*callerWillJoin*/);
    ALOGD("prepare for android.hardware.rpi4gpio@1.0-service");
    if(android::OK !=  service->registerAsService()) {
        ALOGE("Can't register service of Gpio");
        return 1;
    }
    joinRpcThreadpool();
    return 1;
}
