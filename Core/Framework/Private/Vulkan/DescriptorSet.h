#pragma once
#include <memory>
#include <vector>
#include <map>

#if FVCORE_ENABLE_VULKAN
#include <vulkan/vulkan.h>
#include "BufferView.h"
#include "ImageView.h"
#include "Sampler.h"

namespace FV::Vulkan {
    class GraphicsDevice;
    class DescriptorPool;
    class DescriptorSet {
    public:
        DescriptorSet(std::shared_ptr<GraphicsDevice>, std::shared_ptr<DescriptorPool>, VkDescriptorSet);
        virtual ~DescriptorSet();

        using BufferViewObject = std::shared_ptr<BufferView>;
        using ImageViewObject = std::shared_ptr<ImageView>;
        using SamplerObject = std::shared_ptr<Sampler>;

        using ImageLayoutMap = std::map<Image*, VkImageLayout>;
        using ImageViewLayoutMap = std::map<VkImageView, VkImageLayout>;

        struct Binding {
            VkDescriptorSetLayoutBinding layoutBinding;

            // hold resource object ownership
            std::vector<BufferViewObject> bufferViews;
            std::vector<ImageViewObject> imageViews;
            std::vector<SamplerObject> samplers;

            // descriptor infos (for storage of VkWriteDescriptorSets)
            std::vector<VkDescriptorImageInfo> imageInfos;
            std::vector<VkDescriptorBufferInfo> bufferInfos;
            std::vector<VkBufferView> texelBufferViews;

            // pending updates (vkUpdateDescriptorSets)
            VkWriteDescriptorSet write;
            bool valueSet;
        };
        std::vector<Binding> bindings;

        using ImageLayoutMap = std::map<Image*, VkImageLayout>;
        using ImageViewLayoutMap = std::map<VkImageView, VkImageLayout>;

        void collectImageViewLayouts(ImageLayoutMap&, ImageViewLayoutMap&);
        void updateImageViewLayouts(const ImageViewLayoutMap&);

        VkDescriptorSet descriptorSet;
        std::shared_ptr<DescriptorPool> descriptorPool;
        std::shared_ptr<GraphicsDevice> gdevice;
    };
}
#endif //#if FVCORE_ENABLE_VULKAN
