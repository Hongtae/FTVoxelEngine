#pragma once
#include "../include.h"
#include <functional>
#include "RenderCommandEncoder.h"
#include "ComputeCommandEncoder.h"
#include "CopyCommandEncoder.h"

namespace FV
{
    class GraphicsDevice;
    class CommandQueue;
    class CommandBuffer
    {
    public:
        enum class Status
        {
            NotEnqueued = 0,
            Enqueued,
            Committed,
            Scheduled,
            Completed,
            Error,
        };

        virtual ~CommandBuffer() {}

        virtual std::shared_ptr<RenderCommandEncoder> makeRenderCommandEncoder(const RenderPassDescriptor&) = 0;
        virtual std::shared_ptr<ComputeCommandEncoder> makeComputeCommandEncoder() = 0;
        virtual std::shared_ptr<CopyCommandEncoder> makeCopyCommandEncoder() = 0;

        /// Registers one or more callback functions.
        /// However, registered functions can be called from other threads.
        virtual void addCompletedHandler(std::function<void ()>) = 0;
        /// Commit this command buffer to the GPU-Queue (CommandQueue)
        virtual bool commit() = 0;

        virtual std::shared_ptr<CommandQueue> queue() const = 0;
        virtual std::shared_ptr<GraphicsDevice> device() const;
    };
}
