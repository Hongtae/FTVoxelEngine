#include "Extensions.h"
#include "GraphicsDevice.h"
#include "DescriptorSet.h"
#include "DescriptorPool.h"

#if FVCORE_ENABLE_VULKAN
using namespace FV::Vulkan;

DescriptorSet::DescriptorSet(std::shared_ptr<GraphicsDevice> dev,
                             std::shared_ptr<DescriptorPool> pool,
                             VkDescriptorSet ds)
    : gdevice(dev)
    , descriptorSet(ds)
    , descriptorPool(pool)
{
}

DescriptorSet::~DescriptorSet()
{
    gdevice->releaseDescriptorSets(descriptorPool.get(), &descriptorSet, 1);
}

void DescriptorSet::collectImageViewLayouts(ImageLayoutMap& imageLayouts, ImageViewLayoutMap& viewLayouts)
{
    std::map<VkImageView, ImageView*> imageViewMap;
    for (Binding& binding : bindings)
    {
        if (!binding.valueSet)
            continue;

        for (auto& view : binding.imageViews)
        {
            imageViewMap[view->imageView] = view.get();
        }
    }

    for (Binding& binding : bindings)
    {
        if (!binding.valueSet)
            continue;

        const VkWriteDescriptorSet& write = binding.write;
        const VkDescriptorImageInfo* imageInfo = write.pImageInfo;
        if (imageInfo && imageInfo->imageView != VK_NULL_HANDLE)
        {
            auto p = imageViewMap.find(imageInfo->imageView);
            FVASSERT_DEBUG(p != imageViewMap.end());
            if (p != imageViewMap.end())
            {
                ImageView* imageView = p->second;
                FVASSERT_DEBUG(imageView->imageView == imageInfo->imageView);

                Image* image = imageView->image.get();
                VkImageLayout layout = imageInfo->imageLayout;

                if (auto p2 = imageLayouts.find(image); p2 != imageLayouts.end())
                {
                    FVASSERT_DEBUG(p2->second != VK_IMAGE_LAYOUT_UNDEFINED);
                    FVASSERT_DEBUG(imageInfo->imageLayout != VK_IMAGE_LAYOUT_UNDEFINED);

                    if (p2->second != imageInfo->imageLayout)
                    {
                        layout = VK_IMAGE_LAYOUT_GENERAL;
                        p2->second = layout;
                    }
                }
                else
                {
                    imageLayouts[image] = imageInfo->imageLayout;
                }
                viewLayouts[imageInfo->imageView] = layout;
            }
        }
    }
}

void DescriptorSet::updateImageViewLayouts(const ImageViewLayoutMap& imageLayouts)
{
    std::vector<VkWriteDescriptorSet> descriptorWrites;
    descriptorWrites.reserve(bindings.size());

    for (Binding& binding : bindings)
    {
        if (!binding.valueSet)
            continue;

        VkWriteDescriptorSet& write = binding.write;
        FVASSERT_DEBUG(write.dstSet == descriptorSet);
        FVASSERT_DEBUG(write.dstBinding == binding.layoutBinding.binding);

        if (write.pImageInfo)
        {
            bool update = false;
            for (uint32_t i = 0; i < write.descriptorCount; ++i)
            {
                VkDescriptorImageInfo& imageInfo = (VkDescriptorImageInfo&)write.pImageInfo[i];
                if (imageInfo.imageView != VK_NULL_HANDLE)
                {
                    if (auto p = imageLayouts.find(imageInfo.imageView); p != imageLayouts.end())
                    {
                        VkImageLayout layout = p->second;
                        if (imageInfo.imageLayout != layout)
                        {
                            // Update layout
                            imageInfo.imageLayout = layout;
                            update = true;
                        }
                    }
                    else
                    {
                        //imageInfo->imageLayout = VK_IMAGE_LAYOUT_GENERAL;
                        Log::error("Cannot find proper image layout");
                    }
                }
            }
            if (update)
                descriptorWrites.push_back(write);
        }
    }
    if (descriptorWrites.size() > 0)
    {
        vkUpdateDescriptorSets(gdevice->device,
                               (uint32_t)descriptorWrites.size(),
                               descriptorWrites.data(),
                               0,
                               nullptr);
    }
}

#endif //#if FVCORE_ENABLE_VULKAN
