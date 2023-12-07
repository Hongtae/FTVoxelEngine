#pragma once

#if FVCORE_ENABLE_VULKAN
#include <vulkan/vulkan.h>
#include <string>
#include <format>

#define DEF_VK_PFN(name)	PFN_##name name = nullptr
#define GET_INSTANCE_PROC(inst, name)	name = reinterpret_cast<decltype(name)>(vkGetInstanceProcAddr(inst, #name))
#define GET_DEVICE_PROC(dev, name)	name = reinterpret_cast<decltype(name)>(vkGetDeviceProcAddr(dev, #name))

namespace FV {
    struct VulkanInstanceExtensions {
#if 0
        // VK_EXT_debug_report
        DEF_VK_PFN(vkCreateDebugReportCallbackEXT);
        DEF_VK_PFN(vkDestroyDebugReportCallbackEXT);
        DEF_VK_PFN(vkDebugReportMessageEXT);
#endif
        // VK_EXT_debug_utils
        DEF_VK_PFN(vkSetDebugUtilsObjectNameEXT);
        DEF_VK_PFN(vkSetDebugUtilsObjectTagEXT);
        DEF_VK_PFN(vkQueueBeginDebugUtilsLabelEXT);
        DEF_VK_PFN(vkQueueEndDebugUtilsLabelEXT);
        DEF_VK_PFN(vkQueueInsertDebugUtilsLabelEXT);
        DEF_VK_PFN(vkCmdBeginDebugUtilsLabelEXT);
        DEF_VK_PFN(vkCmdEndDebugUtilsLabelEXT);
        DEF_VK_PFN(vkCmdInsertDebugUtilsLabelEXT);
        DEF_VK_PFN(vkCreateDebugUtilsMessengerEXT);
        DEF_VK_PFN(vkDestroyDebugUtilsMessengerEXT);
        DEF_VK_PFN(vkSubmitDebugUtilsMessageEXT);

		// VK_KHR_surface
		DEF_VK_PFN(vkGetPhysicalDeviceSurfaceSupportKHR);
		DEF_VK_PFN(vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
		DEF_VK_PFN(vkGetPhysicalDeviceSurfaceFormatsKHR);
		DEF_VK_PFN(vkGetPhysicalDeviceSurfacePresentModesKHR);

#ifdef VK_USE_PLATFORM_XLIB_KHR
		// VK_KHR_xlib_surface
		DEF_VK_PFN(vkCreateXlibSurfaceKHR);
		DEF_VK_PFN(vkGetPhysicalDeviceXlibPresentationSupportKHR);
#endif
#ifdef VK_USE_PLATFORM_XCB_KHR
		// VK_KHR_xcb_surface
		DEF_VK_PFN(vkCreateXcbSurfaceKHR);
		DEF_VK_PFN(vkGetPhysicalDeviceXcbPresentationSupportKHR);
#endif
#ifdef VK_USE_PLATFORM_WAYLAND_KHR
		// VK_KHR_wayland_surface
		DEF_VK_PFN(vkCreateWaylandSurfaceKHR);
		DEF_VK_PFN(vkGetPhysicalDeviceWaylandPresentationSupportKHR);
#endif
#ifdef VK_USE_PLATFORM_MIR_KHR
		// VK_KHR_mir_surface
		DEF_VK_PFN(vkCreateMirSurfaceKHR);
		DEF_VK_PFN(vkGetPhysicalDeviceMirPresentationSupportKHR);
#endif
#ifdef VK_USE_PLATFORM_ANDROID_KHR
		// VK_KHR_android_surface
		DEF_VK_PFN(vkCreateAndroidSurfaceKHR);
#endif
#ifdef VK_USE_PLATFORM_WIN32_KHR
		// VK_KHR_win32_surface
		DEF_VK_PFN(vkCreateWin32SurfaceKHR);
		DEF_VK_PFN(vkGetPhysicalDeviceWin32PresentationSupportKHR);
#endif

        void load(VkInstance instance) {
#if 0
            // VK_EXT_debug_report
            GET_INSTANCE_PROC(instance, vkCreateDebugReportCallbackEXT);
            GET_INSTANCE_PROC(instance, vkDestroyDebugReportCallbackEXT);
            GET_INSTANCE_PROC(instance, vkDebugReportMessageEXT);
#endif
            // VK_EXT_debug_utils
            GET_INSTANCE_PROC(instance, vkSetDebugUtilsObjectNameEXT);
            GET_INSTANCE_PROC(instance, vkSetDebugUtilsObjectTagEXT);
            GET_INSTANCE_PROC(instance, vkQueueBeginDebugUtilsLabelEXT);
            GET_INSTANCE_PROC(instance, vkQueueEndDebugUtilsLabelEXT);
            GET_INSTANCE_PROC(instance, vkQueueInsertDebugUtilsLabelEXT);
            GET_INSTANCE_PROC(instance, vkCmdBeginDebugUtilsLabelEXT);
            GET_INSTANCE_PROC(instance, vkCmdEndDebugUtilsLabelEXT);
            GET_INSTANCE_PROC(instance, vkCmdInsertDebugUtilsLabelEXT);
            GET_INSTANCE_PROC(instance, vkCreateDebugUtilsMessengerEXT);
            GET_INSTANCE_PROC(instance, vkDestroyDebugUtilsMessengerEXT);
            GET_INSTANCE_PROC(instance, vkSubmitDebugUtilsMessageEXT);

            // VK_KHR_surface
			GET_INSTANCE_PROC(instance, vkGetPhysicalDeviceSurfaceSupportKHR);
			GET_INSTANCE_PROC(instance, vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
			GET_INSTANCE_PROC(instance, vkGetPhysicalDeviceSurfaceFormatsKHR);
			GET_INSTANCE_PROC(instance, vkGetPhysicalDeviceSurfacePresentModesKHR);

#ifdef VK_USE_PLATFORM_XLIB_KHR
			GET_INSTANCE_PROC(instance, vkCreateXlibSurfaceKHR);
			GET_INSTANCE_PROC(instance, vkGetPhysicalDeviceXlibPresentationSupportKHR);
#endif
#ifdef VK_USE_PLATFORM_XCB_KHR
			GET_INSTANCE_PROC(instance, vkCreateXcbSurfaceKHR);
			GET_INSTANCE_PROC(instance, vkGetPhysicalDeviceXcbPresentationSupportKHR);
#endif
#ifdef VK_USE_PLATFORM_WAYLAND_KHR
			GET_INSTANCE_PROC(instance, vkCreateWaylandSurfaceKHR);
			GET_INSTANCE_PROC(instance, vkGetPhysicalDeviceWaylandPresentationSupportKHR);
#endif
#ifdef VK_USE_PLATFORM_MIR_KHR
			GET_INSTANCE_PROC(instance, vkCreateMirSurfaceKHR);
			GET_INSTANCE_PROC(instance, vkGetPhysicalDeviceMirPresentationSupportKHR);
#endif
#ifdef VK_USE_PLATFORM_ANDROID_KHR
			GET_INSTANCE_PROC(instance, vkCreateAndroidSurfaceKHR);
#endif
#ifdef VK_USE_PLATFORM_WIN32_KHR
			GET_INSTANCE_PROC(instance, vkCreateWin32SurfaceKHR);
			GET_INSTANCE_PROC(instance, vkGetPhysicalDeviceWin32PresentationSupportKHR);
#endif
		}
	};

    struct VulkanDeviceExtensions {
#if 0
		// VK_EXT_debug_marker
		DEF_VK_PFN(vkDebugMarkerSetObjectTagEXT);
		DEF_VK_PFN(vkDebugMarkerSetObjectNameEXT);
		DEF_VK_PFN(vkCmdDebugMarkerBeginEXT);
		DEF_VK_PFN(vkCmdDebugMarkerEndEXT);
		DEF_VK_PFN(vkCmdDebugMarkerInsertEXT);
#endif
#if 0
		// VK_KHR_swapchain
		DEF_VK_PFN(vkCreateSwapchainKHR);
		DEF_VK_PFN(vkDestroySwapchainKHR);
		DEF_VK_PFN(vkGetSwapchainImagesKHR);
		DEF_VK_PFN(vkAcquireNextImageKHR);
		DEF_VK_PFN(vkQueuePresentKHR);
#endif
#if 0
		// VK_EXT_extended_dynamic_state
        DEF_VK_PFN(vkCmdSetCullModeEXT);
        DEF_VK_PFN(vkCmdSetFrontFaceEXT);
        DEF_VK_PFN(vkCmdSetPrimitiveTopologyEXT);
        DEF_VK_PFN(vkCmdSetViewportWithCountEXT);
        DEF_VK_PFN(vkCmdSetScissorWithCountEXT);
        DEF_VK_PFN(vkCmdBindVertexBuffers2EXT);
        DEF_VK_PFN(vkCmdSetDepthTestEnableEXT);
        DEF_VK_PFN(vkCmdSetDepthWriteEnableEXT);
        DEF_VK_PFN(vkCmdSetDepthCompareOpEXT);
        DEF_VK_PFN(vkCmdSetDepthBoundsTestEnableEXT);
        DEF_VK_PFN(vkCmdSetStencilTestEnableEXT);
        DEF_VK_PFN(vkCmdSetStencilOpEXT);
#endif
        void load(VkDevice device) {
#if 0
            // VK_EXT_debug_marker
			GET_DEVICE_PROC(device, vkDebugMarkerSetObjectTagEXT);
			GET_DEVICE_PROC(device, vkDebugMarkerSetObjectNameEXT);
			GET_DEVICE_PROC(device, vkCmdDebugMarkerBeginEXT);
			GET_DEVICE_PROC(device, vkCmdDebugMarkerEndEXT);
			GET_DEVICE_PROC(device, vkCmdDebugMarkerInsertEXT);
#endif
#if 0
            // VK_KHR_swapchain
			GET_DEVICE_PROC(device, vkCreateSwapchainKHR);
			GET_DEVICE_PROC(device, vkDestroySwapchainKHR);
			GET_DEVICE_PROC(device, vkGetSwapchainImagesKHR);
			GET_DEVICE_PROC(device, vkAcquireNextImageKHR);
			GET_DEVICE_PROC(device, vkQueuePresentKHR);
#endif
#if 0
            // VK_EXT_extended_dynamic_state
            GET_DEVICE_PROC(device, vkCmdSetCullModeEXT);
            GET_DEVICE_PROC(device, vkCmdSetFrontFaceEXT);
            GET_DEVICE_PROC(device, vkCmdSetPrimitiveTopologyEXT);
            GET_DEVICE_PROC(device, vkCmdSetViewportWithCountEXT);
            GET_DEVICE_PROC(device, vkCmdSetScissorWithCountEXT);
            GET_DEVICE_PROC(device, vkCmdBindVertexBuffers2EXT);
            GET_DEVICE_PROC(device, vkCmdSetDepthTestEnableEXT);
            GET_DEVICE_PROC(device, vkCmdSetDepthWriteEnableEXT);
            GET_DEVICE_PROC(device, vkCmdSetDepthCompareOpEXT);
            GET_DEVICE_PROC(device, vkCmdSetDepthBoundsTestEnableEXT);
            GET_DEVICE_PROC(device, vkCmdSetStencilTestEnableEXT);
            GET_DEVICE_PROC(device, vkCmdSetStencilOpEXT);
#endif
		}
	};

    inline std::string getVkResultString(VkResult r) {
        switch (r) {
#define CASE_STR(c) case c: return #c
            CASE_STR(VK_SUCCESS);
            CASE_STR(VK_NOT_READY);
            CASE_STR(VK_TIMEOUT);
            CASE_STR(VK_EVENT_SET);
            CASE_STR(VK_EVENT_RESET);
            CASE_STR(VK_INCOMPLETE);
            CASE_STR(VK_ERROR_OUT_OF_HOST_MEMORY);
            CASE_STR(VK_ERROR_OUT_OF_DEVICE_MEMORY);
            CASE_STR(VK_ERROR_INITIALIZATION_FAILED);
            CASE_STR(VK_ERROR_DEVICE_LOST);
            CASE_STR(VK_ERROR_MEMORY_MAP_FAILED);
            CASE_STR(VK_ERROR_LAYER_NOT_PRESENT);
            CASE_STR(VK_ERROR_EXTENSION_NOT_PRESENT);
            CASE_STR(VK_ERROR_FEATURE_NOT_PRESENT);
            CASE_STR(VK_ERROR_INCOMPATIBLE_DRIVER);
            CASE_STR(VK_ERROR_TOO_MANY_OBJECTS);
            CASE_STR(VK_ERROR_FORMAT_NOT_SUPPORTED);
            CASE_STR(VK_ERROR_FRAGMENTED_POOL);
            CASE_STR(VK_ERROR_UNKNOWN);
            CASE_STR(VK_ERROR_OUT_OF_POOL_MEMORY);
            CASE_STR(VK_ERROR_INVALID_EXTERNAL_HANDLE);
            CASE_STR(VK_ERROR_FRAGMENTATION);
            CASE_STR(VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS);
            CASE_STR(VK_PIPELINE_COMPILE_REQUIRED);
            CASE_STR(VK_ERROR_SURFACE_LOST_KHR);
            CASE_STR(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR);
            CASE_STR(VK_SUBOPTIMAL_KHR);
            CASE_STR(VK_ERROR_OUT_OF_DATE_KHR);
            CASE_STR(VK_ERROR_INCOMPATIBLE_DISPLAY_KHR);
            CASE_STR(VK_ERROR_VALIDATION_FAILED_EXT);
            CASE_STR(VK_ERROR_INVALID_SHADER_NV);
            CASE_STR(VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT);
            CASE_STR(VK_ERROR_NOT_PERMITTED_KHR);
            CASE_STR(VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT);
            CASE_STR(VK_THREAD_IDLE_KHR);
            CASE_STR(VK_THREAD_DONE_KHR);
            CASE_STR(VK_OPERATION_DEFERRED_KHR);
            CASE_STR(VK_OPERATION_NOT_DEFERRED_KHR);
            CASE_STR(VK_ERROR_COMPRESSION_EXHAUSTED_EXT);
#undef CASE_STR
		}
		return std::format("VkResult({})", int(r));
	}
}
#undef DEF_VK_PFN
#undef GET_INSTANCE_PROC
#undef GET_DEVICE_PROC

namespace std {
    template <> struct formatter<VkResult> : formatter<string> {
        auto format(VkResult arg, format_context& ctx) const {
            return formatter<string>::format(
                FV::getVkResultString(arg), ctx);
        }
    };
}

#endif //#if FVCORE_ENABLE_VULKAN
