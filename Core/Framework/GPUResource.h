#pragma once
#include "../include.h"

namespace FV
{
    enum CPUCacheMode
    {
        CPUCacheModeDefault = 0,
        CPUCacheModeWriteCombined,
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
