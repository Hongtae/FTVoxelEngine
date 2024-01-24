#include <format>
#include <mutex>
#include <condition_variable>
#include "GraphicsDeviceContext.h"
#include "Logger.h"
#include "Application.h"

#include "Private/Vulkan/VulkanInstance.h"

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
        if (auto instance = VulkanInstance::makeInstance(
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
        Log::error("GraphicsDeviceContext creation failed: {}", err.what());
    }
    if (device) {
        return std::make_shared<GraphicsDeviceContext>(device);
    }
    return nullptr;
}

std::shared_ptr<CommandQueue> GraphicsDeviceContext::commandQueue(uint32_t flags) {
    if (auto iter = std::find_if(
        cachedQueues.begin(), cachedQueues.end(), [flags](auto& queue) {
            return (queue->flags() & flags) == flags;
        }); iter != cachedQueues.end()) {
        return *iter;
    }

    auto queue = device->makeCommandQueue(flags);
    if (queue) {
        cachedQueues.push_back(queue);
    }
    return queue;
}

std::shared_ptr<CommandQueue> GraphicsDeviceContext::renderQueue() {
    return commandQueue(CommandQueue::Render);
}

std::shared_ptr<CommandQueue> GraphicsDeviceContext::computeQueue() {
    return commandQueue(CommandQueue::Compute);
}

std::shared_ptr<CommandQueue> GraphicsDeviceContext::copyQueue() {
    return commandQueue(CommandQueue::Copy);
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

        auto cbuffer = queue->makeCommandBuffer();
        auto encoder = cbuffer->makeCopyCommandEncoder();

        encoder->copy(buffer, 0, stgBuffer, 0, buffer->length());
        encoder->endEncoding();
        cbuffer->addCompletedHandler(
            [cond] // copying cond to avoid destruction due to scope out
            {
                std::unique_lock lock(std::get<0>(*cond));
                std::get<1>(*cond).notify_all();
            });

        std::unique_lock lock(std::get<0>(*cond));
        cbuffer->commit();

        if (auto status = std::get<1>(*cond)
            .wait_for(lock, std::chrono::duration<double>(deviceWaitTimeout));
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

std::shared_ptr<GPUBuffer> GraphicsDeviceContext::makeCPUAccessible(
    std::shared_ptr<Texture> texture) {
    if (texture == nullptr) {
        Log::warning("Invalid texture! texture should not be null!");
        return nullptr;
    }

    auto pixelFormat = texture->pixelFormat();
    uint32_t bpp = pixelFormatBytesPerPixel(pixelFormat);
    uint32_t width = texture->width();
    uint32_t height = texture->height();
    auto bufferLength = width * height * bpp;

    auto queue = copyQueue();
    if (queue == nullptr) {
        Log::error("[FATAL] Unable to make command queue!");
        throw std::runtime_error("Unable to make command queue!");
    }

    if (auto buffer = device->makeBuffer(bufferLength,
                                         GPUBuffer::StorageModeShared,
                                         CPUCacheModeDefault);
        buffer != nullptr) {

        auto cond = std::make_shared<
            std::tuple<std::mutex, std::condition_variable>>();

        auto cbuffer = queue->makeCommandBuffer();
        auto encoder = cbuffer->makeCopyCommandEncoder();

        encoder->copy(texture, TextureOrigin{ 0, 0, 0, 0, 0 },
                      buffer, BufferImageOrigin{ 0, width, height },
                      TextureSize{ width, height, 1 });
        encoder->endEncoding();
        cbuffer->addCompletedHandler(
            [cond] // copying cond to avoid destruction due to scope out
            {
                std::unique_lock lock(std::get<0>(*cond));
                std::get<1>(*cond).notify_all();
            });

        std::unique_lock lock(std::get<0>(*cond));
        cbuffer->commit();

        if (auto status = std::get<1>(*cond)
            .wait_for(lock, std::chrono::duration<double>(deviceWaitTimeout));
            status == std::cv_status::timeout) {
            Log::error(
                "The operation timed out. Device did not respond to the command.");
            return nullptr;
        }

        if (buffer->contents() != nullptr) {
            return buffer;
        }
    }
    return nullptr;
}
