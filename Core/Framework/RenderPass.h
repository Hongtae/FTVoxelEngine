#pragma once
#include "../include.h"
#include "Color.h"
#include "Texture.h"
#include <vector>

namespace FV
{
	struct RenderPassAttachmentDescriptor
	{
		enum LoadAction
		{
			LoadActionDontCare = 0,
			LoadActionLoad,
			LoadActionClear,
		};
		enum StoreAction
		{
			StoreActionDontCare = 0,
			StoreActionStore,
		};

		std::shared_ptr<Texture> renderTarget;

		uint32_t mipmapLevel = 0;
		LoadAction loadAction = LoadActionDontCare;
		StoreAction storeAction = StoreActionDontCare;
	};

	struct RenderPassColorAttachmentDescriptor : public RenderPassAttachmentDescriptor
	{
		Color clearColor;
	};

	struct RenderPassDepthStencilAttachmentDescriptor : public RenderPassAttachmentDescriptor
	{
		float clearDepth = 1.0;
		uint32_t clearStencil = 0;
	};

	struct RenderPassDescriptor
	{
		std::vector<RenderPassColorAttachmentDescriptor> colorAttachments;
		RenderPassDepthStencilAttachmentDescriptor depthStencilAttachment;

		size_t numberOfActiveLayers = 0;
	};
}
