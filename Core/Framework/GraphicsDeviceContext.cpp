#include <format>
#include "GraphicsDeviceContext.h"
#include "Logger.h"
#include "Application.h"

#include "Private/Vulkan/Vulkan.h"

using namespace FV;

GraphicsDeviceContext::GraphicsDeviceContext(std::shared_ptr<GraphicsDevice> dev)
    : device(dev) {
}

GraphicsDeviceContext::~GraphicsDeviceContext() {
}

std::shared_ptr<GraphicsDeviceContext> GraphicsDeviceContext::makeDefault() {
    std::shared_ptr<GraphicsDevice> device;

    try {
#if FVCORE_ENABLE_VULKAN
        std::vector<std::string> requiredLayers;
        std::vector<std::string> optionalLayers;
        std::vector<std::string> requiredExtensions;
        std::vector<std::string> optionalExtensions;
        bool enableExtensionsForEnabledLayers = false;
        bool enableLayersForEnabledExtensions = false;
        bool enableValidation = false;
        bool enableDebugUtils = false;

#ifdef FVCORE_DEBUG_ENABLED
        auto args = Application::commandLineArguments();
        if (std::find_if(args.begin(), args.end(),
                         [](auto& arg) {
                             std::string lower;
                             std::transform(arg.begin(), arg.end(), std::back_inserter(lower),
                                            [](auto c) { return std::tolower(c); });
                             return lower.compare("--disable-validation") == 0;
                         }) == args.end()) {
            enableValidation = true;
        }
        enableDebugUtils = true;
#endif
        if (auto instance = Vulkan::VulkanInstance::makeInstance(
            requiredLayers,
            optionalLayers,
            requiredExtensions,
            optionalExtensions,
            enableExtensionsForEnabledLayers,
            enableLayersForEnabledExtensions,
            enableValidation,
            enableDebugUtils); instance) {
            device = instance->makeDevice({}, {});
        }
#endif
    } catch (const std::exception& err) {
        Log::error(std::format("GraphicsDeviceContext creation failed: {}",
                               err.what()));
    }
    if (device) {
        return std::make_shared<GraphicsDeviceContext>(device);
    }
    return nullptr;
}

std::shared_ptr<CommandQueue> GraphicsDeviceContext::renderQueue() {
    if (renderQueues.empty()) {
        if (auto queue = device->makeCommandQueue(CommandQueue::Render); queue) {
            if (queue->flags() & CommandQueue::Render) {
                renderQueues.push_back(queue);
            }
            if (queue->flags() & CommandQueue::Compute) {
                computeQueues.push_back(queue);
            }
            copyQueues.push_back(queue);

            FVASSERT(queue->flags() & CommandQueue::Render);

            return queue;
        }
    }
    return renderQueues.front();
}

std::shared_ptr<CommandQueue> GraphicsDeviceContext::computeQueue() {
    if (computeQueues.empty()) {
        if (auto queue = device->makeCommandQueue(CommandQueue::Compute); queue) {
            if (queue->flags() & CommandQueue::Render) {
                renderQueues.push_back(queue);
            }
            if (queue->flags() & CommandQueue::Compute) {
                computeQueues.push_back(queue);
            }
            copyQueues.push_back(queue);

            FVASSERT(queue->flags() & CommandQueue::Compute);

            return queue;
        }
    }
    return computeQueues.front();
}

std::shared_ptr<CommandQueue> GraphicsDeviceContext::copyQueue() {
    if (copyQueues.empty()) {
        if (auto queue = device->makeCommandQueue(CommandQueue::Copy); queue) {
            if (queue->flags() & CommandQueue::Render) {
                renderQueues.push_back(queue);
            }
            if (queue->flags() & CommandQueue::Compute) {
                computeQueues.push_back(queue);
            }
            copyQueues.push_back(queue);

            return queue;
        }
    }
    return copyQueues.front();
}

std::shared_ptr<GPUBuffer> GraphicsDeviceContext::makeCPUAccessible(
    std::shared_ptr<GPUBuffer> buffer) {
    if (buffer == nullptr) {
        Log::warning("Invalid buffer! buffer should not be null!");
        return nullptr;
    }

    if (buffer->contents() != nullptr)
        return buffer;

    auto queue = copyQueue();
    if (queue == nullptr) {
        Log::error("[FATAL] Unable to make command queue!");
        throw std::runtime_error("Unable to make command queue!");
    }

    if (auto stgBuffer = device->makeBuffer(buffer->length(),
                                            GPUBuffer::StorageModeShared,
                                            CPUCacheModeDefault);
        stgBuffer != nullptr) {

        auto cond = std::make_shared<
            std::tuple<std::mutex, std::condition_variable>>();

        constexpr double timeout = 2.0;

        auto cbuffer = queue->makeCommandBuffer();
        auto encoder = cbuffer->makeCopyCommandEncoder();

        encoder->copy(buffer, 0, stgBuffer, 0, buffer->length());
        encoder->endEncoding();
        cbuffer->addCompletedHandler(
            [cond] // copying cond to avoid destruction due to scope out
            {
                std::get<1>(*cond).notify_all();
            });
        cbuffer->commit();

        std::unique_lock lock(std::get<0>(*cond));
        if (auto status = std::get<1>(*cond)
            .wait_for(lock, std::chrono::duration<double>(timeout));
            status == std::cv_status::timeout) {
            Log::error(
                "The operation timed out. Device did not respond to the command.");
            return nullptr;
        }

        if (stgBuffer->contents() != nullptr) {
            return stgBuffer;
        }
    }
    return nullptr;
}
