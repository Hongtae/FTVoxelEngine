#pragma once
#include "../include.h"

namespace FV {
    enum CPUCacheMode {
        CPUCacheModeDefault = 0,
        CPUCacheModeWriteCombined,
    };

    class GPUEvent {
    public:
        virtual ~GPUEvent() = default;
    };

    class GPUSemaphore {
    public:
        virtual ~GPUSemaphore() = default;
    };
}
