#pragma once
#include <memory>
#include <vector>
#include <map>
#include <string>

#if FVCORE_ENABLE_VULKAN
#include "Extensions.h"

namespace FV::Vulkan
{
    class PhysicalDeviceDescription
    {
    public:
        PhysicalDeviceDescription(VkPhysicalDevice);
        ~PhysicalDeviceDescription();

        enum class DeviceType
        {
            IntegratedGPU,
            DiscreteGPU,
            VirtualGPU,
            CPU,
            Unknown,
        };

        VkPhysicalDevice device;
        uint32_t venderID;
        uint32_t deviceID;

        std::string registryID() const;

        int devicePriority;
        uint64_t deviceMemory;
        uint64_t numGCQueues;
        uint64_t maxQueues;

        std::string name() const;
        std::string description() const;

        VkPhysicalDeviceProperties properties;
        VkPhysicalDeviceExtendedDynamicState3PropertiesEXT extendedDynamicState3Properties;

        VkPhysicalDeviceFeatures features;
        VkPhysicalDeviceTimelineSemaphoreFeatures timelineSemaphoreFeatures;
        VkPhysicalDeviceExtendedDynamicStateFeaturesEXT extendedDynamicStateFeatures;
        VkPhysicalDeviceExtendedDynamicState2FeaturesEXT extendedDynamicState2Features;
        VkPhysicalDeviceExtendedDynamicState3FeaturesEXT extendedDynamicState3Features;

        VkPhysicalDeviceMemoryProperties memory;
        std::vector<VkQueueFamilyProperties> queueFamilies;
        std::map<std::string, uint32_t> extensions;

        bool hasExtension(const std::string& ext)
        {
            return extensions.contains(ext);
        }
    };
}
#endif
