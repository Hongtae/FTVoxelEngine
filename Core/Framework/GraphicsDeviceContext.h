#pragma once
#include "../include.h"
#include <memory>
#include <unordered_map>
#include "GraphicsDevice.h"

namespace FV {
    class FVCORE_API GraphicsDeviceContext {
    public:
        GraphicsDeviceContext(std::shared_ptr<GraphicsDevice>);
        ~GraphicsDeviceContext();

        std::shared_ptr<GraphicsDevice> const device;

        static std::shared_ptr<GraphicsDeviceContext> makeDefault();

        std::shared_ptr<CommandQueue> renderQueue();
        std::shared_ptr<CommandQueue> computeQueue();
        std::shared_ptr<CommandQueue> copyQueue();
    private:
        std::vector<std::shared_ptr<CommandQueue>> renderQueues;
        std::vector<std::shared_ptr<CommandQueue>> computeQueues;
        std::vector<std::shared_ptr<CommandQueue>> copyQueues;
    };
}
