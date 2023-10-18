#include <memory>
#include "../../Logger.h"
#include "Vulkan.h"
#include "GraphicsDevice.h"
#include "Extensions.h"

#if FVCORE_ENABLE_VULKAN
using namespace FV::Vulkan;

std::weak_ptr<FV::Logger> vulkanDebugLogger;

VkBool32 VKAPI_PTR debugUtilsMessengerCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT           messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT                  messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {
    using Level = FV::Log::Level;
    std::string prefix = "";
    Level level = Level::Info;

    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
        prefix += "";
        level = Level::Verbose;
    }
    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
        prefix += "INFO: ";
        level = Level::Info;
    }
    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        prefix += "WARNING: ";
        level = Level::Warning;
    }
    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        prefix += "ERROR: ";
        level = Level::Error;
    }

    std::string type = "";
    if (messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) {
        type += "";
    }
    if (messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) {
        type += "VALIDATION-";
    }
    if (messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) {
        type += "PERFORMANCE-";
    }

    const char* messageID = "";
    if (pCallbackData->pMessageIdName)
        messageID = pCallbackData->pMessageIdName;
    const char* message = "";
    if (pCallbackData->pMessage)
        message = pCallbackData->pMessage;

    if (auto logger = vulkanDebugLogger.lock()) {
        logger->log(
            level,
            std::format("[{}]({:d}){}",
                        messageID,
                        pCallbackData->messageIdNumber,
                        message));
    } else {
        FV::Log::log(
            level,
            std::format("[Vulkan {}{}] [{}]({:d}){}",
                        type, prefix,
                        messageID,
                        pCallbackData->messageIdNumber,
                        message));
    }
    if (level == Level::Error) {
        //FVERROR_ABORT("FATAL ERROR");
        FV_TRAP();
    }
    return VK_FALSE;
}

class VulkanLogger : public FV::Logger {
public:
    VulkanLogger() : Logger("Vulkan") {
    }
};

std::shared_ptr<VulkanInstance> VulkanInstance::makeInstance(
    std::vector<std::string> requiredLayers,
    std::vector<std::string> optionalLayers,
    std::vector<std::string> requiredExtensions,
    std::vector<std::string> optionalExtensions,
    bool enableExtensionsForEnabledLayers,
    bool enableLayersForEnabledExtensions,
    bool enableValidation,
    bool enableDebugUtils,
    VkAllocationCallbacks* allocationCallback) {
    VkApplicationInfo appInfo = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
    appInfo.pApplicationName = "FunTech-V-Core";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "FunTech-V-Core";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3; // Vulkan-1.3

    VkResult err = VK_SUCCESS;

    // checking version.
    uint32_t instanceVersion = 0;
    err = vkEnumerateInstanceVersion(&instanceVersion);
    if (err != VK_SUCCESS) {
        Log::error(std::format("vkEnumerateInstanceVersion failed: {}", err));
        return nullptr;
    }
    Log::info(std::format("Vulkan Instance Version: {:d}.{:d}.{:d}",
                          VK_VERSION_MAJOR(instanceVersion),
                          VK_VERSION_MINOR(instanceVersion),
                          VK_VERSION_PATCH(instanceVersion)));

    // checking layers
    auto availableLayers = []() {
        std::vector<LayerProperties> layers;
        uint32_t layerCount = 0;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> rawLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, rawLayers.data());

        std::transform(rawLayers.begin(), rawLayers.end(),
                       std::back_inserter(layers),
                       [](const VkLayerProperties& props)->LayerProperties {
                           return {
                               props.layerName,
                               props.specVersion,
                               props.implementationVersion,
                               props.description
                           };
                       });
        return layers;
    }();

    auto getLayerExtensionSpecMap = [](const char* layer) {
        std::map<std::string, uint32_t> extensions;
        uint32_t extCount = 0;
        VkResult err = vkEnumerateInstanceExtensionProperties(
            layer, &extCount, nullptr);
        if (err == VK_SUCCESS) {
            if (extCount > 0) {
                std::vector<VkExtensionProperties> rawExtensions(extCount);
                vkEnumerateInstanceExtensionProperties(layer,
                                                       &extCount,
                                                       rawExtensions.data());
                for (auto& ext : rawExtensions) {
                    extensions[ext.extensionName] = ext.specVersion;
                }
            }
        } else {
            Log::error(
                std::format("vkEnumerateInstanceExtensionProperties failed: {}", err));
        }
        return extensions;
    };

    std::shared_ptr output = std::make_shared<VulkanInstance>();
    output->allocationCallback = allocationCallback;
    //output->debugLogger = std::make_shared<VulkanLogger>();
    //vulkanDebugLogger = output->debugLogger;

    for (auto& layer : availableLayers) {
        auto extensions = getLayerExtensionSpecMap(layer.name.c_str());
        for (auto iter = extensions.begin(); iter != extensions.end(); ++iter)
            output->extensionSupportLayers[iter->first].push_back(layer.name);

        output->layers[layer.name] = {
            layer.name,
            layer.specVersion,
            layer.implementationVersion,
            layer.description,
            extensions
        };
    }

    // default extensions
    output->extensions = getLayerExtensionSpecMap(nullptr);
    for (auto iter = output->extensions.begin();
         iter != output->extensions.end(); ++iter)
        output->extensionSupportLayers.try_emplace(iter->first);

    // print info
    if (true) {
        Log::verbose(std::format("Vulkan available layers: {}",
                                 output->layers.size()));
        for (auto& iter : output->layers) {
            auto& layer = iter.second;
            auto spec = std::format("{:d}.{:d}.{:d}",
                                    VK_VERSION_MAJOR(layer.specVersion),
                                    VK_VERSION_MINOR(layer.specVersion),
                                    VK_VERSION_PATCH(layer.specVersion));
            Log::verbose(
                std::format(" -- Layer: {} ({}, spec:{}, implementation: {}",
                            layer.name,
                            layer.description,
                            spec,
                            layer.implementationVersion));
        }
        for (auto& iter : output->extensions) {
            auto& ext = iter.first;
            auto specVersion = output->extensions[ext];
            Log::verbose(
                std::format(" -- Instance extension: {} (Version: {})",
                            ext, specVersion));

        }
    }

    if (enableValidation) {
        requiredLayers.push_back("VK_LAYER_KHRONOS_validation");
        requiredExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    } else if (enableDebugUtils) {
        requiredExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    requiredExtensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
#if VK_USE_PLATFORM_WIN32_KHR
    requiredExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#endif
#if VK_USE_PLATFORM_ANDROID_KHR
    requiredExtensions.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#endif
#if VK_USE_PLATFORM_WAYLAND_KHR
    requiredExtensions.push_back(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
#endif

    // add layers for required extensions
    for (auto& ext : requiredExtensions) {
        if (auto iter = output->extensionSupportLayers.find(ext);
            iter != output->extensionSupportLayers.end()) {
            if (enableLayersForEnabledExtensions) {
                requiredLayers.insert(requiredLayers.end(),
                                      iter->second.begin(), iter->second.end());
            }
        } else {
            Log::warning(std::format(
                "Instance extension: {} not supported, but required.", ext));
        }
    }
    // add layers for optional extensions
    for (auto& ext : optionalExtensions) {
        if (auto iter = output->extensionSupportLayers.find(ext);
            iter != output->extensionSupportLayers.end()) {
            if (enableLayersForEnabledExtensions) {
                optionalLayers.insert(optionalLayers.end(),
                                      iter->second.begin(), iter->second.end());
            }
        } else {
            Log::warning(std::format("Instance extension: {} not supported.",
                                     ext));
        }
    }

    // setup layer, merge extension list
    std::vector<const char*> enabledLayers;
    for (auto& layer : optionalLayers) {
        if (output->layers.contains(layer))
            requiredLayers.push_back(layer);
        else
            Log::warning(std::format("Layer: {} not supported.", layer));
    }
    for (auto& layer : requiredLayers) {
        enabledLayers.push_back(layer.c_str());
        if (output->layers.contains(layer) == false) {
            Log::warning(std::format("Layer: {} not supported, but required",
                                     layer));
        }
    }
    // setup instance extensions!
    std::vector<const char*> enabledExtensions;
    if (enableExtensionsForEnabledLayers) {
        for (auto& item : enabledLayers) {
            if (auto it = output->layers.find(item); it != output->layers.end()) {
                std::transform(it->second.extensions.begin(),
                               it->second.extensions.end(),
                               std::back_inserter(optionalExtensions),
                               [](const auto& pair) {
                                   return pair.first;
                               });
            }
        }
    }
    // merge two extensions
    for (auto& ext : optionalExtensions) {
        if (output->extensionSupportLayers.contains(ext)) {
            requiredExtensions.push_back(ext);
        }
    }
    for (auto& ext : requiredExtensions)
        enabledExtensions.push_back(ext.c_str());

    VkInstanceCreateInfo instanceCreateInfo = {
        VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO
    };
    instanceCreateInfo.pApplicationInfo = &appInfo;

    VkValidationFeatureEnableEXT enabledFeatures[] = {
        VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT,
        VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_RESERVE_BINDING_SLOT_EXT,
        VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT,
    };
    VkValidationFeaturesEXT validationFeatures = {
        VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT
    };

    if (enableValidation) {
        validationFeatures.enabledValidationFeatureCount =
            (uint32_t)std::size(enabledFeatures);
        validationFeatures.pEnabledValidationFeatures = enabledFeatures;
        instanceCreateInfo.pNext = &validationFeatures;
    }

    if (enabledLayers.empty() == false) {
        instanceCreateInfo.enabledLayerCount = (uint32_t)enabledLayers.size();
        instanceCreateInfo.ppEnabledLayerNames = enabledLayers.data();
    }
    if (enabledExtensions.empty() == false) {
        instanceCreateInfo.enabledExtensionCount =
            (uint32_t)enabledExtensions.size();
        instanceCreateInfo.ppEnabledExtensionNames = enabledExtensions.data();
    }

    // create instance!
    VkInstance instance = nullptr;
    err = vkCreateInstance(&instanceCreateInfo, allocationCallback, &instance);
    if (err != VK_SUCCESS) {
        Log::error(
            std::format("vkCreateInstance failed: {}", err));
        return nullptr;
    }

    output->instance = instance;

    if (enabledLayers.empty())
        Log::verbose("VkInstance enabled layers: None");
    else {
        int index = 0;
        for (auto& layer : enabledLayers) {
            Log::verbose(std::format("VkInstance enabled layer[{:d}]: {}",
                                     index, layer));
            index++;
        }
    }
    if (enabledExtensions.empty())
        Log::verbose("VkInstance enabled extensions: None");
    else {
        int index = 0;
        for (auto& ext : enabledExtensions) {
            Log::verbose(std::format("VkInstance enabled extension[{:d}]: {}",
                                     index, ext));
            index++;
        }
    }

    // load extensions
    output->extensionProc.load(instance);

    if (std::find_if(enabledExtensions.begin(), enabledExtensions.end(),
                     [](auto ext) {
                         return strcmp(ext, VK_EXT_DEBUG_UTILS_EXTENSION_NAME)
                             == 0;
                     }) != enabledExtensions.end()) {
        VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo = {
            VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT
        };
        debugUtilsMessengerCreateInfo.messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

        debugUtilsMessengerCreateInfo.messageType =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

        debugUtilsMessengerCreateInfo.pUserData = nullptr;
        debugUtilsMessengerCreateInfo.pfnUserCallback =
            debugUtilsMessengerCallback;

        err = output->extensionProc.vkCreateDebugUtilsMessengerEXT(
            instance,
            &debugUtilsMessengerCreateInfo,
            allocationCallback,
            &output->debugMessenger);

        if (err != VK_SUCCESS)
            Log::error(std::format("vkCreateDebugUtilsMessengerEXT failed: {}", err));
    }

    // get physical device list
    uint32_t gpuCount = 0;
    err = vkEnumeratePhysicalDevices(instance, &gpuCount, nullptr);
    if (err != VK_SUCCESS)
        Log::error(std::format("vkEnumeratePhysicalDevices failed: {}", err));
    if (gpuCount > 0) {
        std::vector<VkPhysicalDevice> physicalDevices(gpuCount);
        err = vkEnumeratePhysicalDevices(instance, &gpuCount,
                                         physicalDevices.data());
        if (err != VK_SUCCESS) {
            Log::error(std::format("vkEnumeratePhysicalDevices failed: {}", err));
            return nullptr;
        }
        uint64_t maxQueueSize = 0;
        for (auto& device : physicalDevices) {
            PhysicalDeviceDescription pd(device);
            maxQueueSize = std::max(maxQueueSize, pd.maxQueues);
            output->physicalDevices.push_back(pd);
        }
    } else {
        Log::error("No Vulkan GPU found.");
        return nullptr;
    }
    // sort device list order by Type,NumQueues,Memory
    std::sort(output->physicalDevices.begin(), output->physicalDevices.end(),
              [](const auto& lhs, const auto& rhs) {
                  if (lhs.devicePriority == rhs.devicePriority) {
                      if (lhs.numGCQueues == rhs.numGCQueues)
                          return lhs.deviceMemory > rhs.deviceMemory;
                      return lhs.numGCQueues > rhs.numGCQueues;
                  }
                  return lhs.devicePriority > rhs.devicePriority;
              });

    for (int index = 0; index < output->physicalDevices.size(); ++index) {
        const auto& device = output->physicalDevices.at(index);
        Log::verbose(std::format("Vulkan physical device[{:d}]: {}",
                                 index, device.description()));
    }
    return output;
}

VulkanInstance::VulkanInstance()
    : instance(nullptr)
    , allocationCallback(nullptr)
    , debugMessenger(VK_NULL_HANDLE)
    , extensionProc{} {
}

VulkanInstance::~VulkanInstance() {
    if (debugMessenger) {
        FVASSERT(instance != nullptr);
        extensionProc.vkDestroyDebugUtilsMessengerEXT(
            instance,
            debugMessenger,
            allocationCallback);
    }
    if (instance) {
        vkDestroyInstance(instance, allocationCallback);
    }
}

std::shared_ptr<FV::GraphicsDevice> VulkanInstance::makeDevice(
    const std::string& identifier,
    std::vector<std::string> requiredExtensions,
    std::vector<std::string> optionalExtensions) {
    if (auto iter = std::find_if(
        physicalDevices.begin(), physicalDevices.end(), [&](const auto& device) {
            return device.registryID().compare(identifier) == 0;
        }); iter != physicalDevices.end()) {
        const auto& device = *iter;
        try {
            return std::make_shared<GraphicsDevice>(shared_from_this(),
                                                    device,
                                                    requiredExtensions,
                                                    optionalExtensions);
        } catch (const std::exception& err) {
            Log::error(std::format("GrahicsDevice creation failed: {}",
                                   err.what()));
        }
    }
    return nullptr;
}

std::shared_ptr<FV::GraphicsDevice> VulkanInstance::makeDevice(
    std::vector<std::string> requiredExtensions,
    std::vector<std::string> optionalExtensions) {
    for (auto& device : this->physicalDevices) {
        try {
            return std::make_shared<GraphicsDevice>(shared_from_this(),
                                                    device,
                                                    requiredExtensions,
                                                    optionalExtensions);
        } catch (const std::exception& err) {
            Log::error(std::format("GrahicsDevice creation failed: {}",
                                   err.what()));
        }
    }
    return nullptr;
}

#endif //#if FVCORE_ENABLE_VULKAN
