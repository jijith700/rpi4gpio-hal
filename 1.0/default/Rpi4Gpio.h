// FIXME: your file license if you have one

#pragma once

#include <android/hardware/rpi4gpio/1.0/IRpi4Gpio.h>
#include <hidl/MQDescriptor.h>
#include <hidl/Status.h>
#include <log/log.h>

namespace android::hardware::rpi4gpio::implementation {

using ::android::hardware::hidl_array;
using ::android::hardware::hidl_memory;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::rpi4gpio::V1_0::LedStatus;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::sp;

struct Rpi4Gpio : public V1_0::IRpi4Gpio {
public:
    Rpi4Gpio();
    ~Rpi4Gpio();
    // Methods from ::android::hardware::rpi4gpio::V1_0::IRpi4Gpio follow.
    Return<LedStatus> get() override;
    Return<int32_t> set(LedStatus val) override;
    Return<void> on() override;
    Return<void> off() override;

    pthread_mutex_t mutexSW = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t conditionSW;

private:
    LedStatus state;
    int valid;

    // Methods from ::android::hidl::base::V1_0::IBase follow.

};

// FIXME: most likely delete, this is only for passthrough implementations
// extern "C" IRpi4Gpio* HIDL_FETCH_IRpi4Gpio(const char* name);

}  // namespace android::hardware::rpi4gpio::implementation
