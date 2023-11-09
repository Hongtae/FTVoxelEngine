#pragma once
#include "../include.h"
#include <vector>
#include "Color.h"
#include "Texture.h"

namespace FV {
    struct RenderPassAttachmentDescriptor {
        enum LoadAction {
            LoadActionDontCare = 0,
            LoadActionLoad,
            LoadActionClear,
        };
        enum StoreAction {
            StoreActionDontCare = 0,
            StoreActionStore,
        };

        std::shared_ptr<Texture> renderTarget;

        uint32_t mipmapLevel = 0;
        LoadAction loadAction = LoadActionDontCare;
        StoreAction storeAction = StoreActionDontCare;
    };
    using RenderPassLoadAction = RenderPassAttachmentDescriptor::LoadAction;
    using RenderPassStoreAction = RenderPassAttachmentDescriptor::StoreAction;

    struct RenderPassColorAttachmentDescriptor : public RenderPassAttachmentDescriptor {
        Color clearColor;
    };

    struct RenderPassDepthStencilAttachmentDescriptor : public RenderPassAttachmentDescriptor {
        float clearDepth = 1.0;
        uint32_t clearStencil = 0;
    };

    struct RenderPassDescriptor {
        std::vector<RenderPassColorAttachmentDescriptor> colorAttachments;
        RenderPassDepthStencilAttachmentDescriptor depthStencilAttachment;

        size_t numberOfActiveLayers = 0;
    };
}
