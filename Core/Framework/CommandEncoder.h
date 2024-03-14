#pragma once
#include "../include.h"

namespace FV {
    class GPUEvent;
    class GPUSemaphore;
    class CommandBuffer;
    class CommandEncoder {
    public:
        virtual ~CommandEncoder() = default;

        virtual void endEncoding() = 0;
        virtual bool isCompleted() const = 0;
        virtual std::shared_ptr<CommandBuffer> commandBuffer() = 0;

        virtual void waitEvent(std::shared_ptr<GPUEvent>) = 0;
        virtual void signalEvent(std::shared_ptr<GPUEvent>) = 0;

        virtual void waitSemaphoreValue(std::shared_ptr<GPUSemaphore>, uint64_t) = 0;
        virtual void signalSemaphoreValue(std::shared_ptr<GPUSemaphore>, uint64_t) = 0;
    };
}