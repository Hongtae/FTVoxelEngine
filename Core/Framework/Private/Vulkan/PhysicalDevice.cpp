#include <algorithm>
#include "PhysicalDevice.h"
#include "Types.h"

#if FVCORE_ENABLE_VULKAN
using namespace FV::Vulkan;

PhysicalDeviceDescription::PhysicalDeviceDescription(VkPhysicalDevice dev)
    : device(dev)
    , numGCQueues(0)
    , maxQueues(0)
    , deviceMemory(0)
    , devicePriority(0) {
    this->properties = {};
    this->features = {};
    this->timelineSemaphoreFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES };
    this->extendedDynamicState3Properties = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_PROPERTIES_EXT };
    this->extendedDynamicStateFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT };
    this->extendedDynamicState2Features = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_2_FEATURES_EXT };
    this->extendedDynamicState3Features = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT };

    this->memory = {};

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    this->queueFamilies.resize(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, this->queueFamilies.data());

    // calculate number of available queues. (Graphics & Compute)
    for (const auto& qf : this->queueFamilies) {
        if (qf.queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT))
            numGCQueues += qf.queueCount;

        maxQueues = std::max(maxQueues, uint64_t(qf.queueCount));
    }

    VkPhysicalDeviceProperties2 properties = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
    properties.pNext = &this->extendedDynamicState3Properties;

    vkGetPhysicalDeviceProperties2(device, &properties);
    this->properties = properties.properties;

    VkPhysicalDeviceMemoryProperties memoryProperties = {};
    vkGetPhysicalDeviceMemoryProperties(device, &memoryProperties);
    this->memory = memoryProperties;

    VkPhysicalDeviceFeatures2 features = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };

    appendNextChain(&features, &this->timelineSemaphoreFeatures);
    appendNextChain(&features, &this->extendedDynamicStateFeatures);
    appendNextChain(&features, &this->extendedDynamicState2Features);
    appendNextChain(&features, &this->extendedDynamicState3Features);

    vkGetPhysicalDeviceFeatures2(device, &features);
    this->features = features.features;

    this->devicePriority = 0;
    switch (properties.properties.deviceType) {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
        this->devicePriority += 1;
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
        this->devicePriority += 1;
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
        this->devicePriority += 1;
    case VK_PHYSICAL_DEVICE_TYPE_CPU:
        this->devicePriority += 1;
    default:    // VK_PHYSICAL_DEVICE_TYPE_OTHER
        break;
    }

    // calculate device memory
    this->deviceMemory = 0;
    for (uint32_t i = 0; i < memoryProperties.memoryHeapCount; ++i) {
        VkMemoryHeap& heap = memoryProperties.memoryHeaps[i];
        if (heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
            this->deviceMemory += heap.size;
    }

    // get list of supported extensions
    uint32_t extCount = 0;
    VkResult err = vkEnumerateDeviceExtensionProperties(device, nullptr, &extCount, nullptr);
    if (err == VK_SUCCESS) {
        if (extCount > 0) {
            std::vector<VkExtensionProperties> rawExtensions(extCount);
            vkEnumerateDeviceExtensionProperties(device, nullptr, &extCount, rawExtensions.data());
            for (const auto& ext : rawExtensions) {
                this->extensions[ext.extensionName] = ext.specVersion;
            }
        }
    } else {
        Log::error(std::format("vkEnumerateDeviceExtensionProperties failed: {}", err));
    }
}

PhysicalDeviceDescription::~PhysicalDeviceDescription() {
}

std::string PhysicalDeviceDescription::registryID() const {
    return std::format("{:08x}{:08x}",
                       properties.vendorID, properties.deviceID);
}

std::string PhysicalDeviceDescription::name() const {
    return properties.deviceName;
}

std::string PhysicalDeviceDescription::description() const {
    const char* deviceType = "Unknown";

    switch (this->properties.deviceType) {
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
        deviceType = "INTEGRATED_GPU";
        break;
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
        deviceType = "DISCRETE_GPU";
        break;
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
        deviceType = "VIRTUAL_GPU";
        break;
    case VK_PHYSICAL_DEVICE_TYPE_CPU:
        deviceType = "CPU";
        break;
    default:
        deviceType = "UNKNOWN";
        break;
    }

    std::string apiVersion = std::format("{:d}.{:d}.{:d}",
                                         VK_VERSION_MAJOR(this->properties.apiVersion),
                                         VK_VERSION_MINOR(this->properties.apiVersion),
                                         VK_VERSION_PATCH(this->properties.apiVersion));

    std::string desc = std::format(
        "[Vulkan] PhysicalDevice(name: {}, identifier: {}, type: {}, API: {}, QueueFamilies: {:d}, NumExtensions: {:d}.",
        this->name(), this->registryID(), deviceType, apiVersion,
        this->queueFamilies.size(),
        this->extensions.size());

    return desc;
}

#endif //#if FVCORE_ENABLE_VULKAN
