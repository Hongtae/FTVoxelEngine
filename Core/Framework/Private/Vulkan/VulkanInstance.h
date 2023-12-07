#pragma once
#include <memory>
#include <vector>
#include <map>
#include "../../GraphicsDevice.h"
#include "../../Logger.h"

#if FVCORE_ENABLE_VULKAN
#include "VulkanPhysicalDevice.h"
#include "VulkanExtensions.h"

namespace FV {
    struct LayerProperties {
        std::string name;
        uint32_t specVersion;
        uint32_t implementationVersion;
        std::string description;
        std::map<std::string, uint32_t> extensions;
    };

    class VulkanInstance : public std::enable_shared_from_this<VulkanInstance> {
    public:
        VulkanInstance();
        ~VulkanInstance();

        std::shared_ptr<GraphicsDevice> makeDevice(
            const std::string& identifier,
            std::vector<std::string> requiredExtensions,
            std::vector<std::string> optionalExtensions);

        std::shared_ptr<GraphicsDevice> makeDevice(
            std::vector<std::string> requiredExtensions,
            std::vector<std::string> optionalExtensions);

        std::map<std::string, LayerProperties> layers;
        std::map<std::string, uint32_t> extensions;
        std::map<std::string, std::vector<std::string>> extensionSupportLayers;
        std::vector<VulkanPhysicalDeviceDescription> physicalDevices;

        static std::shared_ptr<VulkanInstance> makeInstance(
            std::vector<std::string> requiredLayers,
            std::vector<std::string> optionalLayers,
            std::vector<std::string> requiredExtensions,
            std::vector<std::string> optionalExtensions,
            bool enableExtensionsForEnabledLayers = false,
            bool enableLayersForEnabledExtensions = false,
            bool enableValidation = false,
            bool enableDebugUtils = false,
            VkAllocationCallbacks* allocationCallback = nullptr);

        VkAllocationCallbacks* allocationCallback;

        VulkanInstanceExtensions extensionProc;
        VkInstance instance;

    private:
        VkDebugUtilsMessengerEXT debugMessenger;
        std::shared_ptr<Logger> debugLogger;
    };
}
#endif // #if FVCORE_ENABLE_VULKAN

