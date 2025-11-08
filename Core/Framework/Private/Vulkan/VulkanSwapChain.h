#pragma once
#include <memory>
#include <mutex>
#include <vector>
#include "../../SwapChain.h"
#include "../../Window.h"

#if FVCORE_ENABLE_VULKAN
#include <vulkan/vulkan.h>
#include "VulkanCommandQueue.h"
#include "VulkanImageView.h"
#include "VulkanSemaphore.h"

namespace FV {
    class VulkanCommandQueue;
    class VulkanSwapChain : public SwapChain {
    public:
        VulkanSwapChain(std::shared_ptr<VulkanCommandQueue>, std::shared_ptr<Window>);
        ~VulkanSwapChain();

        bool setup();
        bool updateDevice();
        void setupFrame();

        void setPixelFormat(PixelFormat) override;
        PixelFormat pixelFormat() const override;

        RenderPassDescriptor currentRenderPassDescriptor() override;
        size_t maximumBufferCount() const override;

        bool present(GPUEvent**, size_t) override;

        std::shared_ptr<CommandQueue> queue() const override { return cqueue; }

        bool enableVSync;

    private:
        uint64_t frameCount;
        uint32_t imageIndex;

        std::vector<VkSemaphore> acquireSemaphores;
        std::vector<VkSemaphore> submitSemaphores;
        uint32_t numberOfSwapchainImages;
        std::vector<std::shared_ptr<VulkanImageView>> imageViews;

        VkSwapchainKHR swapchain;
        VkSurfaceKHR surface;
        VkSurfaceFormatKHR surfaceFormat;
        std::vector<VkSurfaceFormatKHR> availableSurfaceFormats;

        std::shared_ptr<Window> window;
        std::shared_ptr<VulkanCommandQueue> cqueue;

        mutable std::mutex lock;
        bool deviceReset;	// recreate swapchain
        Size cachedResolution;

        std::optional<RenderPassDescriptor> renderPassDescriptor;

        void updateDeviceIfNeeded();
        void onWindowEvent(const Window::WindowEvent&);
    };
}
#endif //#if FVCORE_ENABLE_VULKAN
