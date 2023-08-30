#include "Extensions.h"
#include "GraphicsDevice.h"
#include "ShaderBindingSet.h"
#include "DescriptorPool.h"

#if FVCORE_ENABLE_VULKAN
using namespace FV::Vulkan;

ShaderBindingSet::ShaderBindingSet(std::shared_ptr<GraphicsDevice> dev,
                                   VkDescriptorSetLayout layout,
                                   const DescriptorPoolID& poolID,
                                   const VkDescriptorSetLayoutCreateInfo& createInfo)
    : gdevice(dev)
    , descriptorSetLayout(layout)
    , poolID(poolID)
    , layoutFlags(createInfo.flags)
{
    FVASSERT_DEBUG(descriptorSetLayout != VK_NULL_HANDLE);

    bindings.reserve(createInfo.bindingCount);
    for (uint32_t i = 0; i < createInfo.bindingCount; ++i)
    {
        const VkDescriptorSetLayoutBinding& binding = createInfo.pBindings[i];

        DescriptorBinding ds = { binding };
        ds.valueSet = false;
        bindings.push_back(ds);
    }
}

ShaderBindingSet::~ShaderBindingSet()
{
    vkDestroyDescriptorSetLayout(gdevice->device, descriptorSetLayout, gdevice->allocationCallbacks());
}

std::shared_ptr<DescriptorSet> ShaderBindingSet::makeDescriptorSet() const
{
    std::shared_ptr<DescriptorSet> descriptorSet = gdevice->makeDescriptorSet(descriptorSetLayout, poolID);
    FVASSERT_DEBUG(descriptorSet);

    descriptorSet->bindings = this->bindings;

    std::vector<VkWriteDescriptorSet> descriptorWrites;
    descriptorWrites.reserve(descriptorSet->bindings.size());

    for (DescriptorBinding& binding : descriptorSet->bindings)
    {
        if (!binding.valueSet)
            continue;

        VkWriteDescriptorSet& write = binding.write;
        write.dstSet = descriptorSet->descriptorSet;
        if (write.pImageInfo)
            write.pImageInfo = binding.imageInfos.data();
        if (write.pBufferInfo)
            write.pBufferInfo = binding.bufferInfos.data();
        if (write.pTexelBufferView)
            write.pTexelBufferView = binding.texelBufferViews.data();

        descriptorWrites.push_back(write);
    }

    FVASSERT_DEBUG(descriptorWrites.empty() == false);

    vkUpdateDescriptorSets(gdevice->device,
                           (uint32_t)descriptorWrites.size(),
                           descriptorWrites.data(),
                           0,
                           nullptr);

    return descriptorSet;
}

ShaderBindingSet::DescriptorBinding* ShaderBindingSet::findDescriptorBinding(uint32_t binding)
{
    for (DescriptorBinding& b : bindings)
    {
        if (b.layoutBinding.binding == binding)
        {
            return &b;
        }
    }
    return nullptr;
}

void ShaderBindingSet::setBuffer(uint32_t binding, std::shared_ptr<FV::GPUBuffer> bufferObject, uint64_t offset, uint64_t length)
{
    BufferInfo bufferInfo = { bufferObject, offset, length };
    return setBufferArray(binding, 1, &bufferInfo);
}

void ShaderBindingSet::setBufferArray(uint32_t binding, uint32_t numBuffers, BufferInfo* bufferArray)
{
    DescriptorBinding* descriptorBinding = findDescriptorBinding(binding);
    if (descriptorBinding)
    {
        descriptorBinding->valueSet = false;
        descriptorBinding->bufferInfos.clear();
        descriptorBinding->imageInfos.clear();
        descriptorBinding->texelBufferViews.clear();
        descriptorBinding->bufferViews.clear();
        descriptorBinding->imageViews.clear();
        descriptorBinding->samplers.clear();

        const VkDescriptorSetLayoutBinding& descriptor = descriptorBinding->layoutBinding;

        uint32_t startingIndex = 0;
        uint32_t availableItems = std::min(numBuffers, descriptor.descriptorCount - startingIndex);
        FVASSERT_DEBUG(availableItems <= numBuffers);

        VkWriteDescriptorSet write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        write.dstSet = VK_NULL_HANDLE;
        write.dstBinding = descriptor.binding;
        write.dstArrayElement = startingIndex;
        write.descriptorCount = availableItems;
        write.descriptorType = descriptor.descriptorType;

        FVASSERT_DEBUG(descriptorBinding->bufferInfos.empty());
        FVASSERT_DEBUG(descriptorBinding->texelBufferViews.empty());

        switch (descriptor.descriptorType)
        {
        case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
        case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
            // bufferView (pTexelBufferView)
            descriptorBinding->texelBufferViews.reserve(availableItems);
            for (uint32_t i = 0; i < availableItems; ++i)
            {
                auto bufferView = std::dynamic_pointer_cast<BufferView>(bufferArray[i].buffer);
                FVASSERT_DEBUG(bufferView);
                FVASSERT_DEBUG(bufferView->bufferView);
                descriptorBinding->texelBufferViews.push_back(bufferView->bufferView);
            }
            write.pTexelBufferView = descriptorBinding->texelBufferViews.data();
            break;
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
        case  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
            // buffer (pBufferInfo)
            descriptorBinding->bufferInfos.reserve(availableItems);
            for (uint32_t i = 0; i < availableItems; ++i)
            {
                auto bufferView = std::dynamic_pointer_cast<BufferView>(bufferArray[i].buffer);
                FVASSERT_DEBUG(bufferView);
                auto& buffer = bufferView->buffer;
                FVASSERT_DEBUG(buffer);
                FVASSERT_DEBUG(buffer->buffer != VK_NULL_HANDLE);

                VkDescriptorBufferInfo bufferInfo = {};
                bufferInfo.buffer = buffer->buffer;
                bufferInfo.offset = bufferArray[i].offset;
                bufferInfo.range = bufferArray[i].length;

                descriptorBinding->bufferInfos.push_back(bufferInfo);
            }
            write.pBufferInfo = descriptorBinding->bufferInfos.data();
            break;
        default:
            Log::error("Invalid type!");
            FVASSERT_DESC_DEBUG(0, "Invalid descriptor type!");
            return;
        }

        // take ownership of resource.
        descriptorBinding->bufferViews.reserve(availableItems);

        for (uint32_t i = 0; i < availableItems; ++i)
        {
            auto bufferView = std::dynamic_pointer_cast<BufferView>(bufferArray[i].buffer);
            FVASSERT_DEBUG(bufferView);
            descriptorBinding->bufferViews.push_back(bufferView);
        }
        descriptorBinding->write = write;
        descriptorBinding->valueSet = true;
    }
}

void ShaderBindingSet::setTexture(uint32_t binding, std::shared_ptr<FV::Texture> textureObject)
{
    return setTextureArray(binding, 1, &textureObject);
}

void ShaderBindingSet::setTextureArray(uint32_t binding, uint32_t numTextures, std::shared_ptr<FV::Texture>* textureArray)
{
    DescriptorBinding* descriptorBinding = findDescriptorBinding(binding);
    if (descriptorBinding)
    {
        //descriptorBinding->descriptorWrites.Clear();
        descriptorBinding->bufferInfos.clear();
        //descriptorBinding->imageInfos.clear();
        descriptorBinding->texelBufferViews.clear();
        descriptorBinding->bufferViews.clear();
        descriptorBinding->imageViews.clear();
        //descriptorBinding->samplers.clear();

        const VkDescriptorSetLayoutBinding& descriptor = descriptorBinding->layoutBinding;

        uint32_t startingIndex = 0;
        uint32_t availableItems = std::min(numTextures, descriptor.descriptorCount - startingIndex);
        FVASSERT_DEBUG(availableItems <= numTextures);

        if (!descriptorBinding->valueSet)
        {
            VkWriteDescriptorSet write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
            write.dstSet = VK_NULL_HANDLE;
            write.dstBinding = descriptor.binding;
            write.dstArrayElement = startingIndex;
            write.descriptorCount = availableItems;  // number of descriptors to update.
            write.descriptorType = descriptor.descriptorType;
            descriptorBinding->write = write;
            descriptorBinding->valueSet = true;
        }
        VkWriteDescriptorSet& write = descriptorBinding->write;
        if (write.pImageInfo == nullptr)
        {
            descriptorBinding->samplers.clear();
            descriptorBinding->imageInfos.clear();
        }
        write.dstArrayElement = startingIndex;
        write.descriptorCount = availableItems;

        auto getImageLayout = [](VkDescriptorType type, PixelFormat pixelFormat)
        {
            VkImageLayout imageLayout = VK_IMAGE_LAYOUT_UNDEFINED; /* imageView->LayerLayout(0);*/
            switch (type)
            {
            case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
                imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                break;
            case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                imageLayout = VK_IMAGE_LAYOUT_GENERAL;
                break;
            case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
                imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                break;
            default:
                imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            }
            return imageLayout;
        };

        switch (descriptor.descriptorType)
        {
            // pImageInfo
        case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
            descriptorBinding->imageViews.reserve(availableItems);
            for (uint32_t i = 0; i < availableItems; ++i)
            {
                if (i >= descriptorBinding->imageInfos.size())
                {
                    VkDescriptorImageInfo info = {};
                    auto index = descriptorBinding->imageInfos.size();
                    descriptorBinding->imageInfos.push_back(info);
                    FVASSERT_DEBUG(index == i);
                }

                auto imageView = std::dynamic_pointer_cast<ImageView>(textureArray[i]);
                FVASSERT_DEBUG(imageView);
                FVASSERT_DEBUG(imageView->imageView != VK_NULL_HANDLE);

                if (descriptor.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
                {
                    if (!(imageView->image->usage & VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT))
                    {
                        Log::error("ImageView image does not have usage flag:VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT");
                    }
                }

                VkDescriptorImageInfo& imageInfo = descriptorBinding->imageInfos.at(i);
                imageInfo.imageView = imageView->imageView;
                imageInfo.imageLayout = getImageLayout(descriptor.descriptorType, imageView->image->pixelFormat());

                descriptorBinding->imageViews.push_back(imageView);
            }
            write.pImageInfo = descriptorBinding->imageInfos.data();
            break;
        default:
            Log::error("Invalid type!");
            FVASSERT_DESC_DEBUG(0, "Invalid descriptor type!");
            return;
        }
    }
}

void ShaderBindingSet::setSamplerState(uint32_t binding, std::shared_ptr<FV::SamplerState> samplerState)
{
    return setSamplerStateArray(binding, 1, &samplerState);
}

void ShaderBindingSet::setSamplerStateArray(uint32_t binding, uint32_t numSamplers, std::shared_ptr<FV::SamplerState>* samplerArray) 
{
    DescriptorBinding* descriptorBinding = findDescriptorBinding(binding);
    if (descriptorBinding)
    {
        //descriptorBinding->descriptorWrites.clear();
        descriptorBinding->bufferInfos.clear();
        //descriptorBinding->imageInfos.clear();
        descriptorBinding->texelBufferViews.clear();
        descriptorBinding->bufferViews.clear();
        //descriptorBinding->imageViews.clear();
        descriptorBinding->samplers.clear();

        const VkDescriptorSetLayoutBinding& descriptor = descriptorBinding->layoutBinding;

        uint32_t startingIndex = 0;
        uint32_t availableItems = std::min(numSamplers, descriptor.descriptorCount - startingIndex);
        FVASSERT_DEBUG(availableItems <= numSamplers);

        if (!descriptorBinding->valueSet)
        {
            VkWriteDescriptorSet write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
            write.dstSet = VK_NULL_HANDLE;
            write.dstBinding = descriptor.binding;
            write.dstArrayElement = startingIndex;
            write.descriptorCount = availableItems;  // number of descriptors to update.
            write.descriptorType = descriptor.descriptorType;
            descriptorBinding->write = write;
            descriptorBinding->valueSet = true;
        }

        VkWriteDescriptorSet& write = descriptorBinding->write;
        if (write.pImageInfo == nullptr)
        {
            descriptorBinding->imageViews.clear();
            descriptorBinding->imageInfos.clear();
        }
        write.dstArrayElement = startingIndex;
        write.descriptorCount = availableItems;

        switch (descriptor.descriptorType)
        {
        case VK_DESCRIPTOR_TYPE_SAMPLER:
        case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            descriptorBinding->samplers.reserve(availableItems);
            for (uint32_t i = 0; i < availableItems; ++i)
            {
                VkDescriptorImageInfo* imageInfo = nullptr;
                if (i < descriptorBinding->imageInfos.size())
                {
                    imageInfo = &descriptorBinding->imageInfos.at(i);
                }
                else
                {
                    VkDescriptorImageInfo info = {};
                    auto index = descriptorBinding->imageInfos.size();
                    descriptorBinding->imageInfos.push_back(info);
                    imageInfo = &descriptorBinding->imageInfos.at(index);
                    FVASSERT_DEBUG(index == i);
                }

                auto sampler = std::dynamic_pointer_cast<Sampler>(samplerArray[i]);
                FVASSERT_DEBUG(sampler);
                FVASSERT_DEBUG(sampler->sampler != VK_NULL_HANDLE);

                imageInfo->sampler = sampler->sampler;
                descriptorBinding->samplers.push_back(sampler);
            }
            write.pImageInfo = descriptorBinding->imageInfos.data();
            break;
        default:
            Log::error("Invalid type!");
            FVASSERT_DESC_DEBUG(0, "Invalid descriptor type!");
            return;
        }
    }
}

#endif //#if FVCORE_ENABLE_VULKAN
