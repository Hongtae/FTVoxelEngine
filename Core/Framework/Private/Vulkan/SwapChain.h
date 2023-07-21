#pragma once
#include <memory>
#include <mutex>
#include <vector>
#include "../../SwapChain.h"
#include "../../Window.h"

#if FVCORE_ENABLE_VULKAN
#include <vulkan/vulkan.h>
#include "CommandQueue.h"
#include "ImageView.h"

namespace FV::Vulkan
{
	class CommandQueue;
	class SwapChain : public FV::SwapChain
	{
	public:
		SwapChain(std::shared_ptr<CommandQueue>, std::shared_ptr<FV::Window>);
		~SwapChain();

		bool setup();
		bool updateDevice();
		void setupFrame();

		void setPixelFormat(PixelFormat) override;
		PixelFormat pixelFormat() const override;

		RenderPassDescriptor currentRenderPassDescriptor() override;
        size_t maximumBufferCount() const override;

		bool present(FV::GPUEvent**, size_t) override;

        std::shared_ptr<FV::CommandQueue> queue() const override { return cqueue; }

		bool enableVSync;
		VkSurfaceFormatKHR surfaceFormat;
		VkSurfaceKHR surface;
		VkSwapchainKHR swapchain;
		std::vector<VkSurfaceFormatKHR> availableSurfaceFormats;

        VkSemaphore frameReadySemaphore;

        std::vector<std::shared_ptr<ImageView>> imageViews;

		std::shared_ptr<FV::Window> window;
		std::shared_ptr<CommandQueue> cqueue;

		mutable std::mutex lock;
		bool deviceReset;	// recreate swapchain

		uint32_t frameIndex;
		RenderPassDescriptor renderPassDescriptor;

		void onWindowEvent(const FV::Window::WindowEvent&);
	};
}
#endif //#if FVCORE_ENABLE_VULKAN
