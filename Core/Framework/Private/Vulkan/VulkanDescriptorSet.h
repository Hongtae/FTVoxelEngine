#pragma once
#include <memory>
#include <vector>
#include <map>

#if FVCORE_ENABLE_VULKAN
#include <vulkan/vulkan.h>
#include "VulkanBufferView.h"
#include "VulkanImageView.h"
#include "VulkanSampler.h"

namespace FV {
    class VulkanGraphicsDevice;
    class VulkanDescriptorPool;
    class VulkanDescriptorSet {
    public:
        VulkanDescriptorSet(std::shared_ptr<VulkanGraphicsDevice>, std::shared_ptr<VulkanDescriptorPool>, VkDescriptorSet);
        virtual ~VulkanDescriptorSet();

        using BufferViewObject = std::shared_ptr<VulkanBufferView>;
        using ImageViewObject = std::shared_ptr<VulkanImageView>;
        using SamplerObject = std::shared_ptr<VulkanSampler>;

        using ImageLayoutMap = std::map<VulkanImage*, VkImageLayout>;
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

        using ImageLayoutMap = std::map<VulkanImage*, VkImageLayout>;
        using ImageViewLayoutMap = std::map<VkImageView, VkImageLayout>;

        void collectImageViewLayouts(ImageLayoutMap&, ImageViewLayoutMap&);
        void updateImageViewLayouts(const ImageViewLayoutMap&);

        VkDescriptorSet descriptorSet;
        std::shared_ptr<VulkanDescriptorPool> descriptorPool;
        std::shared_ptr<VulkanGraphicsDevice> gdevice;
    };
}
#endif //#if FVCORE_ENABLE_VULKAN
