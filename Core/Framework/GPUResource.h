#pragma once
#include "../include.h"

namespace FV
{
    enum CPUCacheMode
    {
        CPUCacheModeReadWrite = 0,
        CPUCacheModeWriteOnly,
    };

    class GPUEvent
    {
    public:
        virtual ~GPUEvent() {}
    };

    class GPUSemaphore
    {
    public:
        virtual ~GPUSemaphore() {}
    };
}
