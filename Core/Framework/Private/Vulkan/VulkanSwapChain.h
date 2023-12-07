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
        VkSurfaceFormatKHR surfaceFormat;
        VkSurfaceKHR surface;
        VkSwapchainKHR swapchain;
        std::vector<VkSurfaceFormatKHR> availableSurfaceFormats;

        VkSemaphore frameReadySemaphore;

        std::vector<std::shared_ptr<VulkanImageView>> imageViews;

        std::shared_ptr<Window> window;
        std::shared_ptr<VulkanCommandQueue> cqueue;

        mutable std::mutex lock;
        bool deviceReset;	// recreate swapchain

        uint32_t frameIndex;
        RenderPassDescriptor renderPassDescriptor;

        void onWindowEvent(const Window::WindowEvent&);
    };
}
#endif //#if FVCORE_ENABLE_VULKAN
