#pragma once
#include "../include.h"
#include "RenderPass.h"
#include "PixelFormat.h"
#include "GPUResource.h"

namespace FV
{
	class CommandQueue;
	class SwapChain
	{
	public:
		virtual ~SwapChain() {}

		virtual PixelFormat pixelFormat() const = 0;
		virtual void setPixelFormat(PixelFormat) = 0;

		virtual RenderPassDescriptor currentRenderPassDescriptor() = 0;
		virtual size_t maximumBufferCount() const = 0;

		virtual std::shared_ptr<CommandQueue> queue() const = 0;

		virtual bool present(GPUEvent** waitEvents, size_t numEvents) = 0;

		bool present() { return present(nullptr, 0); }
	};

}
