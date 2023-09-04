#pragma once
#include "../include.h"

namespace FV {
    class GraphicsDevice;
    class GPUBuffer {
    public:
        virtual ~GPUBuffer() {}

        enum StorageMode {
            StorageModeShared = 0,	// accessible to both the CPU and the GPU
            StorageModePrivate,		// only accessible to the GPU
        };

        virtual void* contents() = 0;
        virtual void flush() = 0;
        virtual size_t length() const = 0;

        virtual std::shared_ptr<GraphicsDevice> device() const = 0;
    };
}
