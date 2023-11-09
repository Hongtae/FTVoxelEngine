#pragma once
#include "../include.h"
#include <vector>
#include "GraphicsDevice.h"

namespace FV {
    class FVCORE_API GraphicsDeviceContext {
    public:
        GraphicsDeviceContext(std::shared_ptr<GraphicsDevice>);
        ~GraphicsDeviceContext();

        std::shared_ptr<GraphicsDevice> const device;

        static std::shared_ptr<GraphicsDeviceContext> makeDefault();

        std::shared_ptr<CommandQueue> commandQueue(uint32_t flags = 0);
        std::shared_ptr<CommandQueue> renderQueue();
        std::shared_ptr<CommandQueue> computeQueue();
        std::shared_ptr<CommandQueue> copyQueue();

        std::shared_ptr<GPUBuffer> makeCPUAccessible(std::shared_ptr<GPUBuffer>);
        std::shared_ptr<GPUBuffer> makeCPUAccessible(std::shared_ptr<Texture>);

        constexpr static double deviceWaitTimeout = 2.0;

    private:
        std::vector<std::shared_ptr<CommandQueue>> cachedQueues;
    };
}
