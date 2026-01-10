#define VOLK_IMPLEMENTATION
#define VMA_IMPLEMENTATION

#include "GraphicsDevice_Vulkan.h"
#include "Graphics/GraphicsTypes.h"
#include "Graphics/Vulkan/GraphicsHelpers_Vulkan.h"
#include "Graphics/Vulkan/GraphicsTypes_Vulkan.h"
#include "Core/Logger.h"
#include "Data/ArenaAllocator.h"

#include <imgui.h>
#include <imgui_impl_vulkan.h>

#include <Windows.h>
#include <vector>
#include <stdlib.h>

#define SR_MAX_CBV_SRV_UAV_DESCRIPTORS 65536
#define SR_MAX_SAMPLER_DESCRIPTORS     2048

namespace {
	// TODO: Move elsewhere
	template<typename T>
	constexpr T align_to(T value, T alignment) {
		return ((value + alignment - T(1)) / alignment) * alignment;
	}

	constexpr const char* REQUIRED_INSTANCE_EXTS[] = {
		VK_KHR_SURFACE_EXTENSION_NAME,
		VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
		VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME,
	};
	constexpr const char* REQUIRED_DEVICE_EXTS[] = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
		VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
		VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
		VK_KHR_SPIRV_1_4_EXTENSION_NAME,
		VK_EXT_MESH_SHADER_EXTENSION_NAME,
		VK_EXT_MUTABLE_DESCRIPTOR_TYPE_EXTENSION_NAME,
	};
}

internal SRGFXDeviceVTable SRGFXDevice_Vulkan_VTable = {
	.destroy_device              = SRGFXVulkan_DestroyDevice,
	.get_frame_index             = SRGFXVulkan_GetFrameIndex,
	.create_swapchain            = SRGFXVulkan_CreateSwapchain,
	.create_pipeline             = SRGFXVulkan_CreatePipeline,
	.create_buffer               = SRGFXVulkan_CreateBuffer,
	.create_texture              = SRGFXVulkan_CreateTexture,
	.create_sampler              = SRGFXVulkan_CreateSampler,
	.destroy_swapchain           = SRGFXVulkan_DestroySwapchain,
	.destroy_pipeline            = SRGFXVulkan_DestroyPipeline,
	.destroy_resource            = SRGFXVulkan_DestroyResource,
	.bind_pipeline               = SRGFXVulkan_BindPipeline,
	.bind_viewport               = SRGFXVulkan_BindViewport,
	.bind_vertex_buffer          = SRGFXVulkan_BindVertexBuffer,
	.bind_index_buffer           = SRGFXVulkan_BindIndexBuffer,
	.bind_root_constant_buffer   = SRGFXVulkan_BindRootConstantBuffer,
	.push_constants              = SRGFXVulkan_PushConstants,
	.barrier                     = SRGFXVulkan_Barrier,
	.begin_frame                 = SRGFXVulkan_BeginFrame,
	.begin_command_list          = SRGFXVulkan_BeginCommandList,
	.begin_render_pass_swapchain = SRGFXVulkan_BeginRenderPassSwapchain,
	.begin_render_pass           = SRGFXVulkan_BeginRenderPass,
	.end_render_pass_swapchain   = SRGFXVulkan_EndRenderPassSwapchain,
	.end_render_pass             = SRGFXVulkan_EndRenderPass,
	.submit_command_lists        = SRGFXVulkan_SubmitCommandLists,
	.draw                        = SRGFXVulkan_Draw,
	.draw_indexed                = SRGFXVulkan_DrawIndexed,
	.draw_instanced              = SRGFXVulkan_DrawInstanced,
	.dispatch_mesh               = SRGFXVulkan_DispatchMesh,
	.get_descriptor_index_srv    = SRGFXVulkan_GetDescriptorIndexSRV,
	.get_shader_compile_target   = SRGFXVulkan_GetShaderCompileTarget,
	.wait_for_gpu                = SRGFXVulkan_WaitForGPU,
	.flush_initial_uploads       = SRGFXVulkan_FlushInitialUploads,
	.setup_imgui_init_info       = SRGFXVulkan_SetupImGuiInitInfo
};

struct SRGFXDeviceVulkan {
	VkDebugUtilsMessengerEXT debug_messenger;
	VkInstance instance;
	VkSurfaceKHR surface;
	VkPhysicalDevice physical_device;
	VkDevice device;
	VkCommandPool cmd_pool_upload;
	VkCommandBuffer cmd_buffer_upload;
	VkCommandPool cmd_pools[SRQueue_COUNT][SR_GFX_FRAMES_IN_FLIGHT];
	VkQueue cmd_queues[SRQueue_COUNT];
	u32 cmd_queue_indices[SRQueue_COUNT];
	VkSemaphore frame_fences[SRQueue_COUNT];
	VkSemaphore semaphores_image_available[SR_GFX_FRAMES_IN_FLIGHT];
	VkSemaphore semaphores_render_finished[SR_MAX_SWAPCHAIN_IMAGES];
	VkFence acquire_fence;
	VkDescriptorPool descriptor_pool;
	VkDescriptorSet descriptor_set_bindless;
	VkDescriptorSetLayout descriptor_set_layout_bindless;
	VkDescriptorSetLayout descriptor_set_layout_push;
	VmaAllocator vma_allocator;

	SRDescriptorHeap_Vulkan* descriptor_heap_cbv_srv_uav;
	SRDescriptorHeap_Vulkan* descriptor_heap_sampler;
	SRPipeline_Vulkan* active_pipeline;
	SRDestructionHandler_Vulkan* destruction_handler;

	SRArena* arena_general;
	SRArena* arena_upload;
	SRWindow* window;
	u64 frame_done_values[SRQueue_COUNT][SR_GFX_FRAMES_IN_FLIGHT];
	SRCmdList_Vulkan cmd_lists[SRQueue_COUNT][SR_GFX_FRAMES_IN_FLIGHT];
	u32 frame_index;
	u32 image_index;
	u64 frame_counter;

	bool is_upload_cmd_buffer_recording;
	bool is_debug_utils_available;
};

internal VKAPI_ATTR VkBool32 VKAPI_CALL SRGFXDeviceVulkan_DebugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData
) {
	if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
		SRLOG_ERROR_CAT(SRLOG_CAT_VULKAN, pCallbackData->pMessage);
		return VK_FALSE;
	}

	SRLOG_DEBUG_CAT(SRLOG_CAT_VULKAN, pCallbackData->pMessage);
	return VK_FALSE;
}

internal void SRGFXDeviceVulkan_CreateInstance(SRGFXDeviceVulkan* dev) {
	SR_VK_CHECK(volkInitialize(), "Volk initialization");
	SRLOG_INFO_CAT(SRLOG_CAT_VULKAN, "Volk successfully initialized");

	VkApplicationInfo appInfo = {
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pApplicationName = "Stingray",
		.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),
		.pEngineName = "Stingray",
		.engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),
		.apiVersion = VK_API_VERSION_1_4
	};

	VkInstanceCreateInfo instanceInfo = {
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo = &appInfo
	};

	// Instance layers
	u32 numInstanceLayers;
	vkEnumerateInstanceLayerProperties(&numInstanceLayers, nullptr);
	std::vector<VkLayerProperties> instanceLayers(static_cast<size_t>(numInstanceLayers));
	SR_VK_CHECK(vkEnumerateInstanceLayerProperties(&numInstanceLayers, instanceLayers.data()), "Instance layer enumeration");

	SRLOG_INFO_CAT(SRLOG_CAT_VULKAN, "Found %u instance layers", numInstanceLayers);
	for (const auto& layer : instanceLayers) {
		SRLOG_DEBUG_CAT(SRLOG_CAT_VULKAN, "\t%s (specVersion=%u)", layer.layerName, layer.specVersion);
	}

#ifdef _DEBUG
	// Enable VK_LAYER_KHRONOS_validation instance layer if available
	const char* validationLayerName = "VK_LAYER_KHRONOS_validation";
	bool validationLayerFound = false;
	for (const auto& layer : instanceLayers) {
		if (strcmp(layer.layerName, validationLayerName) == 0) {
			validationLayerFound = true;
			break;
		}
	}

	if (validationLayerFound) {
		instanceInfo.enabledLayerCount = 1;
		instanceInfo.ppEnabledLayerNames = &validationLayerName;
	}
	else {
		SRLOG_WARN_CAT(SRLOG_CAT_VULKAN, "VK_LAYER_KHRONOS_validation layer not available. Debug information will be limited.");
		instanceInfo.enabledLayerCount = 0;
		instanceInfo.ppEnabledLayerNames = nullptr;
	}
#else
	instanceInfo.enabledLayerCount = 0;
	instanceInfo.ppEnabledLayerNames = nullptr;
#endif

	// Instance extensions
	u32 numInstanceExts;
	vkEnumerateInstanceExtensionProperties(nullptr, &numInstanceExts, nullptr);
	std::vector<VkExtensionProperties> instanceExts(static_cast<size_t>(numInstanceExts));
	SR_VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &numInstanceExts, instanceExts.data()), "Instance extension enumeration");

	SRLOG_INFO_CAT(SRLOG_CAT_VULKAN, "Found %u instance extensions", numInstanceExts);
	for (const auto& ext : instanceExts) {
		SRLOG_DEBUG_CAT(SRLOG_CAT_VULKAN, "\t%s (specVersion=%u)", ext.extensionName, ext.specVersion);
	}

	// Check that required instance extensions are available
	std::vector<const char*> enabledExts;
	for (const char* reqExt : REQUIRED_INSTANCE_EXTS) {
		bool extFound = false;

		for (const auto& ext : instanceExts) {
			if (strcmp(ext.extensionName, reqExt) == 0) {
				extFound = true;
				break;
			}
		}

		if (!extFound) {
			SRLOG_CRITICAL_CAT(SRLOG_CAT_VULKAN, "Missing required Vulkan instance extension: %s", reqExt);
			throw std::runtime_error("Missing required Vulkan extension");
		}

		enabledExts.push_back(reqExt);
	}

#ifdef _DEBUG
	// Add debug utils if available
	for (const auto& ext : instanceExts) {
		if (strcmp(ext.extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0) {
			enabledExts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
			dev->is_debug_utils_available = true;
			break;
		}
	}

	// Create instance-level debug messenger, only used during creation
	VkDebugUtilsMessengerCreateInfoEXT debugMessengerInfo;

	if (dev->is_debug_utils_available) {
		debugMessengerInfo = {
			.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
			.messageSeverity = (
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT
			),
			.messageType = (
				VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT
			),
			.pfnUserCallback = SRGFXDeviceVulkan_DebugCallback
		};
		instanceInfo.pNext = &debugMessengerInfo;
	}
	else {
		SRLOG_WARN_CAT(SRLOG_CAT_VULKAN, "VK_EXT_debug_utils not available. Debug messenger will not be created");
	}
#endif

	instanceInfo.enabledExtensionCount = static_cast<u32>(enabledExts.size());
	instanceInfo.ppEnabledExtensionNames = enabledExts.data();

	// TODO: Investigate custom Vulkan allocator
	SR_VK_CHECK(vkCreateInstance(&instanceInfo, nullptr, &dev->instance), "Instance creation");

	volkLoadInstanceOnly(dev->instance);
}

internal void SRGFXDeviceVulkan_CreateDebugMessenger(SRGFXDeviceVulkan* dev) {
#ifdef _DEBUG
	if (!dev->is_debug_utils_available) {
		return;
	}

	auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
		vkGetInstanceProcAddr(dev->instance, "vkCreateDebugUtilsMessengerEXT")
		);
	if (func == nullptr) {
		SRLOG_ERROR_CAT(SRLOG_CAT_VULKAN, "vkGetInstanceProcAddr could not be obtained");
		return;
	}

	VkDebugUtilsMessengerCreateInfoEXT createInfo = {
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
		.messageSeverity = (
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT
		),
		.messageType = (
			VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT
		),
		.pfnUserCallback = SRGFXDeviceVulkan_DebugCallback
	};
	SR_VK_CHECK(func(dev->instance, &createInfo, nullptr, &dev->debug_messenger), "Debug messenger creation");
#else
	return;
#endif
}

internal void SRGFXDeviceVulkan_CreateSurface(SRGFXDeviceVulkan* dev) {
	VkWin32SurfaceCreateInfoKHR win32SurfaceInfo = {
		.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
		.hinstance = (HINSTANCE)GetModuleHandle(nullptr),
		.hwnd = (HWND)SRWindow_GetInternalHandle(dev->window)
	};

	SR_VK_CHECK(vkCreateWin32SurfaceKHR(dev->instance, &win32SurfaceInfo, nullptr, &dev->surface), "Win32 surface creation");
}

internal void SRGFXDeviceVulkan_CreateDevice(SRGFXDeviceVulkan* dev) {
	u32 numDevices = 0;
	SR_VK_CHECK(vkEnumeratePhysicalDevices(dev->instance, &numDevices, nullptr), "Physical device enumeration");

	if (numDevices == 0) {
		SRLOG_CRITICAL_CAT(SRLOG_CAT_VULKAN, "No GPU with Vulkan support was found");
		throw std::runtime_error("Vulkan error: No GPU with Vulkan support was found");
	}

	std::vector<VkPhysicalDevice> devices(numDevices);
	SR_VK_CHECK(vkEnumeratePhysicalDevices(dev->instance, &numDevices, devices.data()), "Physical device enumeration");

	SRLOG_DEBUG_CAT(SRLOG_CAT_VULKAN, "Found %u potential device(s). Enumerating...", numDevices);
	u32 pickedDeviceIdx = ~0;
	const char* deviceName = nullptr;
	for (u32 i = 0; i < numDevices; ++i) {
		std::vector<std::string> missing;
		const auto REQUIRE = [&](bool condition, const char* str) {
			if (!condition) { missing.emplace_back(str); }
			};

		VkPhysicalDevice& device = devices[i];

		// Device properties
		// TODO: Store ray tracing properties for later use (alignments needed)
		VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingProps = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
		};
		VkPhysicalDeviceDriverProperties deviceDriverProps = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES,
			.pNext = &rayTracingProps
		};
		VkPhysicalDeviceIDProperties idProps = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES,
			.pNext = &deviceDriverProps,
		};
		VkPhysicalDeviceProperties2 deviceProps2 = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
			.pNext = &idProps
		};
		vkGetPhysicalDeviceProperties2(device, &deviceProps2);

		// TODO: Add more checks relating to the properties, such as
		// supported ray recursion depth, anisotropic and so on.

		deviceName = deviceProps2.properties.deviceName;
		bool isApi14Plus = (
			VK_VERSION_MAJOR(deviceProps2.properties.apiVersion) > 1 ||
			(VK_VERSION_MAJOR(deviceProps2.properties.apiVersion) == 1 && VK_VERSION_MINOR(deviceProps2.properties.apiVersion) >= 4)
		);

		if (!isApi14Plus) {
			SRLOG_DEBUG_CAT(
				SRLOG_CAT_VULKAN,
				"[GPU%u] %s REJECTED. Device does not support Vulkan 1.3 or higher", i, deviceName
			);
			continue;
		}

		// Device extensions
		u32 numDeviceExts;
		SR_VK_CHECK(vkEnumerateDeviceExtensionProperties(device, nullptr, &numDeviceExts, nullptr), "Device extensions enumeration");
		std::vector<VkExtensionProperties> deviceExts(numDeviceExts);
		SR_VK_CHECK(vkEnumerateDeviceExtensionProperties(device, nullptr, &numDeviceExts, deviceExts.data()), "Device extensions enumeration");

		// TODO: Some extensions might be optional in the future, so look into
		// making this logic better
		std::vector<const char*> enabledExts;
		for (const char* reqExt : REQUIRED_DEVICE_EXTS) {
			bool isReqExtSupported = false;

			for (const auto& ext : deviceExts) {
				if (strcmp(ext.extensionName, reqExt) == 0) {
					enabledExts.push_back(reqExt);
					isReqExtSupported = true;
					break;
				}
			}

			REQUIRE(isReqExtSupported, (std::string("extension: ") + reqExt).c_str());
		}

		// Device features
		VkPhysicalDeviceMutableDescriptorTypeFeaturesEXT mutableDescriptorTypeFeatures = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MUTABLE_DESCRIPTOR_TYPE_FEATURES_EXT,
			.pNext = nullptr
		};
		VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeatures = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
			.pNext = &mutableDescriptorTypeFeatures
		};
		VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeatures = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
			.pNext = &rtPipelineFeatures
		};
		VkPhysicalDeviceMeshShaderFeaturesEXT meshShaderFeatures = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT,
			.pNext = &asFeatures
		};
		VkPhysicalDeviceVulkan14Features vk14Features = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES,
			.pNext = &meshShaderFeatures
		};
		VkPhysicalDeviceVulkan13Features vk13Features = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
			.pNext = &vk14Features
		};
		VkPhysicalDeviceVulkan12Features vk12Features = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
			.pNext = &vk13Features
		};
		VkPhysicalDeviceFeatures2 deviceFeatures = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
			.pNext = &vk12Features
		};
		vkGetPhysicalDeviceFeatures2(device, &deviceFeatures);

		REQUIRE(vk14Features.pushDescriptor, "feature: pushDescriptor");
		REQUIRE(vk13Features.synchronization2, "feature: synchronization2");
		REQUIRE(vk13Features.dynamicRendering, "feature: dynamicRendering");
		REQUIRE(vk12Features.timelineSemaphore, "feature: timelineSemaphore");
		REQUIRE(vk12Features.bufferDeviceAddress, "feature: bufferDeviceAddress");
		REQUIRE(rtPipelineFeatures.rayTracingPipeline, "feature: rayTracingPipeline");
		REQUIRE(asFeatures.accelerationStructure, "feature: accelerationStructure");
		REQUIRE(meshShaderFeatures.meshShader, "feature: meshShader");
		REQUIRE(vk12Features.descriptorIndexing, "feature: descriptorIndexing");
		REQUIRE(vk12Features.descriptorBindingPartiallyBound, "feature: descriptorBindingPartiallyBound");
		REQUIRE(vk12Features.runtimeDescriptorArray, "feature: runtimeDescriptorArray");
		REQUIRE(vk12Features.descriptorBindingSampledImageUpdateAfterBind, "feature: descriptorBindingSampledImageUpdateAfterBind");
		REQUIRE(vk12Features.descriptorBindingStorageBufferUpdateAfterBind, "feature: descriptorBindingStorageBufferUpdateAfterBind");
		REQUIRE(vk12Features.descriptorBindingStorageImageUpdateAfterBind, "feature: descriptorBindingStorageImageUpdateAfterBind");
		REQUIRE(vk12Features.descriptorBindingUniformBufferUpdateAfterBind, "feature: descriptorBindingUniformBufferUpdateAfterBind");
		REQUIRE(vk12Features.shaderSampledImageArrayNonUniformIndexing, "feature: shaderSampledImageArrayNonUniformIndexing");
		REQUIRE(vk12Features.shaderStorageBufferArrayNonUniformIndexing, "feature: shaderStorageBufferArrayNonUniformIndexing");
		REQUIRE(vk12Features.shaderStorageImageArrayNonUniformIndexing, "feature: shaderStorageImageArrayNonUniformIndexing");
		REQUIRE(vk12Features.shaderUniformBufferArrayNonUniformIndexing, "feature: shaderUniformBufferArrayNonUniformIndexing");
		REQUIRE(asFeatures.descriptorBindingAccelerationStructureUpdateAfterBind, "feature: descriptorBindingAccelerationStructureUpdateAfterBind");
		REQUIRE(deviceFeatures.features.samplerAnisotropy, "feature: samplerAnisotropy");
		REQUIRE(mutableDescriptorTypeFeatures.mutableDescriptorType, "feature: mutableDescriptorType");

		// Set enabled device features
		VkPhysicalDeviceMutableDescriptorTypeFeaturesEXT enableMutableDescriptorTypeFeat = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MUTABLE_DESCRIPTOR_TYPE_FEATURES_EXT,
			.pNext = nullptr,
			.mutableDescriptorType = VK_TRUE
		};
		VkPhysicalDeviceRayTracingPipelineFeaturesKHR enableRTFeat = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
			.pNext = &enableMutableDescriptorTypeFeat,
			.rayTracingPipeline = VK_TRUE
		};
		VkPhysicalDeviceAccelerationStructureFeaturesKHR enableASFeat = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
			.pNext = &enableRTFeat,
			.accelerationStructure = VK_TRUE,
			.descriptorBindingAccelerationStructureUpdateAfterBind = VK_TRUE
		};
		VkPhysicalDeviceMeshShaderFeaturesEXT enableMSFeat = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT,
			.pNext = &enableASFeat,
			.meshShader = VK_TRUE
		};
		VkPhysicalDeviceVulkan14Features enableVk14Feat = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES,
			.pNext = &enableMSFeat,
			.pushDescriptor = VK_TRUE
		};
		VkPhysicalDeviceVulkan13Features enableVk13Feat = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
			.pNext = &enableVk14Feat,
			.synchronization2 = VK_TRUE,
			.dynamicRendering = VK_TRUE,
		};
		VkPhysicalDeviceVulkan12Features enableVk12Feat = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
			.pNext = &enableVk13Feat,
			.descriptorIndexing = VK_TRUE,
			.shaderUniformBufferArrayNonUniformIndexing = VK_TRUE,
			.shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
			.shaderStorageBufferArrayNonUniformIndexing = VK_TRUE,
			.shaderStorageImageArrayNonUniformIndexing = VK_TRUE,
			.descriptorBindingUniformBufferUpdateAfterBind = VK_TRUE,
			.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE,
			.descriptorBindingStorageImageUpdateAfterBind = VK_TRUE,
			.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE,
			.descriptorBindingPartiallyBound = VK_TRUE,
			.runtimeDescriptorArray = VK_TRUE,
			.timelineSemaphore = VK_TRUE,
			.bufferDeviceAddress = VK_TRUE
		};
		VkPhysicalDeviceFeatures2 enableDeviceFeat = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
			.pNext = &enableVk12Feat,
			.features = {
				.samplerAnisotropy = VK_TRUE,
			}
		};

		// Queue families
		u32 numQueueFamilies;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &numQueueFamilies, nullptr);
		std::vector<VkQueueFamilyProperties> queueFamilies(numQueueFamilies);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &numQueueFamilies, queueFamilies.data());

		u32 universalQueueFamilyIdx = ~0; // Graphics + Compute + Copy (REQUIRED)
		u32 dedicatedComputeQueueFamilyIdx = ~0; // REQUIRED
		u32 dedicatedCopyQueueFamilyIdx = ~0; // OPTIONAL, though might be made REQUIRED in the future

		for (u32 i = 0; i < numQueueFamilies; ++i) {
			const auto& family = queueFamilies[i];

			// NOTE: In Stingray, we require a "universal" queue to exist, i.e.
			// a queue that supports Graphics + Compute + Copy operations. The
			// Vulkan spec tells us that: "at least one queue family of at least
			// one physical device exposed by the implementation must support
			// both graphics and compute operations".
			// Meaning that a Graphics + Compute queue MUST exist.
			// 
			// Furthermore, the spec tells us that:
			// "All commands that are allowed on a queue that supports transfer
			// operations are also allowed on a queue that supports either
			// graphics or compute operations. Thus, if the capabilities of a
			// queue family include VK_QUEUE_GRAPHICS_BIT or
			// VK_QUEUE_COMPUTE_BIT, then reporting the VK_QUEUE_TRANSFER_BIT
			// capability separately for that queue family is optional."
			//
			// The wording of that sentence is a bit confusing, but more or less
			// it tells us that a queue supporting Graphics + Compute + Copy
			// operations MUST exist. In the real world, queue family 0 will
			// always support this. Though due to some odd drivers this might
			// not always be the case. This means that we only need to check for
			// a queue family that has GRAPHCIS and COMPUTE bits, since that
			// also implies it supporting TRANSFER.
			bool hasGraphicsBit = (family.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
			bool hasComputeBit = (family.queueFlags & VK_QUEUE_COMPUTE_BIT) != 0;
			bool hasCopyBit = (family.queueFlags & VK_QUEUE_TRANSFER_BIT) != 0;

			if (hasGraphicsBit && hasComputeBit) { // Universal queue
				// Check present support
				VkBool32 hasPresentSupport = VK_FALSE;
				SR_VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(device, i, dev->surface, &hasPresentSupport), "Query presentation support");

				if (hasPresentSupport == VK_TRUE && universalQueueFamilyIdx == ~0) {
					universalQueueFamilyIdx = i;
				}
				continue;
			}
			if (hasComputeBit && !hasGraphicsBit) { // Dedicated compute queue (may also have TRANSFER)
				if (dedicatedComputeQueueFamilyIdx == ~0) {
					dedicatedComputeQueueFamilyIdx = i;
				}
				continue;
			}
			if (hasCopyBit && !hasGraphicsBit && !hasComputeBit) { // Dedicated copy queue
				if (dedicatedCopyQueueFamilyIdx == ~0) {
					dedicatedCopyQueueFamilyIdx = i;
				}
			}
		}

		REQUIRE(universalQueueFamilyIdx != ~0, "queue family: universal queue (graphics + compute + copy + present)");
		REQUIRE(dedicatedComputeQueueFamilyIdx != ~0, "queue family: dedicated compute queue");

		if (!missing.empty()) {
			SRLOG_WARN_CAT(SRLOG_CAT_VULKAN, "[GPU%u] %s REJECTED. Missing %zu requirement(s):", i, deviceName, missing.size());

			for (const auto& str : missing) {
				SRLOG_WARN_CAT(SRLOG_CAT_VULKAN, "\t%s", str.c_str());
			}

			continue;
		}

		dev->cmd_queue_indices[SRQueue_Universal] = universalQueueFamilyIdx;
		dev->cmd_queue_indices[SRQueue_Compute] = dedicatedComputeQueueFamilyIdx;
		dev->cmd_queue_indices[SRQueue_Copy] = dedicatedCopyQueueFamilyIdx;

		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		f32 queuePriority = 1.0f; // TODO: Might not always be the best?

		VkDeviceQueueCreateInfo univeralQueueInfo = {
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.queueFamilyIndex = universalQueueFamilyIdx,
			.queueCount = 1,
			.pQueuePriorities = &queuePriority
		};
		queueCreateInfos.push_back(univeralQueueInfo);

		VkDeviceQueueCreateInfo dedicatedComputeQueueInfo = {
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.queueFamilyIndex = dedicatedComputeQueueFamilyIdx,
			.queueCount = 1,
			.pQueuePriorities = &queuePriority
		};
		queueCreateInfos.push_back(dedicatedComputeQueueInfo);

		if (dedicatedCopyQueueFamilyIdx != ~0) {
			VkDeviceQueueCreateInfo dedicatedCopyQueueInfo = {
				.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
				.queueFamilyIndex = dedicatedCopyQueueFamilyIdx,
				.queueCount = 1,
				.pQueuePriorities = &queuePriority
			};
			queueCreateInfos.push_back(dedicatedCopyQueueInfo);
		}

		// TODO: Perhaps have requirements for available present modes? FIFO is always guaranteed at least
		VkDeviceCreateInfo deviceInfo = {
			.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			.pNext = &enableDeviceFeat,
			.queueCreateInfoCount = static_cast<u32>(queueCreateInfos.size()),
			.pQueueCreateInfos = queueCreateInfos.data(),
			.enabledExtensionCount = static_cast<u32>(enabledExts.size()),
			.ppEnabledExtensionNames = enabledExts.data(),
		};

		SR_VK_CHECK(vkCreateDevice(device, &deviceInfo, nullptr, &dev->device), "Device creation");
		pickedDeviceIdx = i;
		break;
	}

	if (pickedDeviceIdx == ~0) {
		SRLOG_CRITICAL_CAT(SRLOG_CAT_VULKAN, "No suitable GPU found");
		throw std::runtime_error("VULKAN ERROR: No suitable GPU found");
	}

	SRLOG_DEBUG_CAT(SRLOG_CAT_VULKAN, "Picked [GPU%u] %s", pickedDeviceIdx, deviceName);

	dev->physical_device = devices[pickedDeviceIdx];

	volkLoadDevice(dev->device);
	vkGetDeviceQueue(dev->device, dev->cmd_queue_indices[SRQueue_Universal], 0, &dev->cmd_queues[SRQueue_Universal]);
	vkGetDeviceQueue(dev->device, dev->cmd_queue_indices[SRQueue_Compute], 0, &dev->cmd_queues[SRQueue_Compute]);
	vkGetDeviceQueue(dev->device, dev->cmd_queue_indices[SRQueue_Copy], 0, &dev->cmd_queues[SRQueue_Copy]);
}

internal void SRGFXDeviceVulkan_CreateMemoryAllocator(SRGFXDeviceVulkan* dev) {
	VmaVulkanFunctions volkFunctions = {
		vkGetInstanceProcAddr,
		vkGetDeviceProcAddr,
		vkGetPhysicalDeviceProperties,
		vkGetPhysicalDeviceMemoryProperties,
		vkAllocateMemory,
		vkFreeMemory,
		vkMapMemory,
		vkUnmapMemory,
		vkFlushMappedMemoryRanges,
		vkInvalidateMappedMemoryRanges,
		vkBindBufferMemory,
		vkBindImageMemory,
		vkGetBufferMemoryRequirements,
		vkGetImageMemoryRequirements,
		vkCreateBuffer,
		vkDestroyBuffer,
		vkCreateImage,
		vkDestroyImage,
		vkCmdCopyBuffer,
		vkGetBufferMemoryRequirements2,
		vkGetImageMemoryRequirements2,
		vkBindBufferMemory2,
		vkBindImageMemory2,
		vkGetPhysicalDeviceMemoryProperties2,
		vkGetDeviceBufferMemoryRequirements,
		vkGetDeviceImageMemoryRequirements
	};

	VmaAllocatorCreateInfo allocatorInfo = {
		.physicalDevice = dev->physical_device,
		.device = dev->device,
		.preferredLargeHeapBlockSize = 0, // 256 MB default
		.pAllocationCallbacks = nullptr,
		.pDeviceMemoryCallbacks = nullptr,
		.pHeapSizeLimit = nullptr,
		.pVulkanFunctions = &volkFunctions,
		.instance = dev->instance,
		.vulkanApiVersion = VK_API_VERSION_1_4,
		.pTypeExternalMemoryHandleTypes = nullptr
	};

	SR_VK_CHECK(vmaCreateAllocator(&allocatorInfo, &dev->vma_allocator), "Create Vulkan Memory Allocator");
}

internal void SRGFXDeviceVulkan_CreateCommandPools(SRGFXDeviceVulkan* dev) {
	VkCommandPoolCreateInfo poolInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, // TODO: Look into whether VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT might OCCASIONALLY be useful
	};

	for (u32 q = 0; q < SRQueue_COUNT; ++q) {
		poolInfo.queueFamilyIndex = dev->cmd_queue_indices[q];

		for (u32 f = 0; f < SR_GFX_FRAMES_IN_FLIGHT; ++f) {
			SR_VK_CHECK(vkCreateCommandPool(dev->device, &poolInfo, nullptr, &dev->cmd_pools[q][f]), "Command pool creation");
		}
	}

	// TEMPORARY
	poolInfo.queueFamilyIndex = dev->cmd_queue_indices[SRQueue_Copy];
	SR_VK_CHECK(vkCreateCommandPool(dev->device, &poolInfo, nullptr, &dev->cmd_pool_upload), "Create upload command pool");

	// Create initial upload command buffer
	VkCommandBufferAllocateInfo allocInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = dev->cmd_pool_upload,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1
	};

	SR_VK_CHECK(vkAllocateCommandBuffers(dev->device, &allocInfo, &dev->cmd_buffer_upload), "Upload command buffer creation");
}

internal void SRGFXDeviceVulkan_CreateSyncObjects(SRGFXDeviceVulkan* dev) {
	// Frame timeline semaphore
	VkSemaphoreTypeCreateInfo timelineInfo = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
		.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
		.initialValue = 0
	};

	VkSemaphoreCreateInfo semaphoreInfo = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		.pNext = &timelineInfo
	};

	for (u32 q = 0; q < SRQueue_COUNT; ++q) {
		SR_VK_CHECK(vkCreateSemaphore(dev->device, &semaphoreInfo, nullptr, &(dev->frame_fences[q])), "Timeline semaphore creation");
	}

	// Image available and render finished semaphores
	semaphoreInfo.pNext = nullptr;
	for (u32 f = 0; f < SR_GFX_FRAMES_IN_FLIGHT; ++f) {
		SR_VK_CHECK(vkCreateSemaphore(dev->device, &semaphoreInfo, nullptr, &dev->semaphores_image_available[f]), "Image-available semaphore creation");
	}

	for (u32 b = 0; b < 3; ++b) {
		SR_VK_CHECK(vkCreateSemaphore(dev->device, &semaphoreInfo, nullptr, &dev->semaphores_render_finished[b]), "Render-finished semaphore creation");
	}

	// Swapchain acquire fence
	VkFenceCreateInfo fenceInfo = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	SR_VK_CHECK(vkCreateFence(dev->device, &fenceInfo, nullptr, &dev->acquire_fence), "Create swapchain acquire-fence");
}

internal void SRGFXDeviceVulkan_CreateDescriptors(SRGFXDeviceVulkan* dev) {
	dev->descriptor_heap_cbv_srv_uav = SRDescriptorHeap_Vulkan_Create(dev->arena_general, VK_DESCRIPTOR_TYPE_MUTABLE_EXT, SR_MAX_CBV_SRV_UAV_DESCRIPTORS);
	dev->descriptor_heap_sampler   = SRDescriptorHeap_Vulkan_Create(dev->arena_general, VK_DESCRIPTOR_TYPE_SAMPLER, SR_MAX_SAMPLER_DESCRIPTORS);

	// Bindless descriptors (set 0)
	std::vector<SRDescriptorHeap_Vulkan*> descriptorHeaps = {
		dev->descriptor_heap_cbv_srv_uav,
		dev->descriptor_heap_sampler,
	};
	std::vector<VkDescriptorPoolSize> poolSizes;
	std::vector<VkDescriptorBindingFlags> bindingFlags;
	std::vector<VkDescriptorSetLayoutBinding> layoutBindings;
	poolSizes.reserve(descriptorHeaps.size());
	bindingFlags.reserve(descriptorHeaps.size());
	layoutBindings.reserve(descriptorHeaps.size());

	for (size_t i = 0; i < descriptorHeaps.size(); ++i) {
		SRDescriptorHeap_Vulkan* heap = descriptorHeaps[i];
		VkDescriptorType descriptorType = heap->type;
		u32 descriptorCount = heap->capacity;

		VkDescriptorPoolSize poolSize = { descriptorType, descriptorCount };
		VkDescriptorBindingFlags flags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
		VkDescriptorSetLayoutBinding layoutBinding = {
			.binding = static_cast<u32>(i),
			.descriptorType = heap->type,
			.descriptorCount = heap->capacity,
			.stageFlags = VK_SHADER_STAGE_ALL,
			.pImmutableSamplers = nullptr
		};

		poolSizes.push_back(poolSize);
		bindingFlags.push_back(flags);
		layoutBindings.push_back(layoutBinding);
	}

	// Descriptor pool
	VkDescriptorPoolCreateInfo poolInfo = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
		.maxSets = 1,
		.poolSizeCount = static_cast<u32>(poolSizes.size()),
		.pPoolSizes = poolSizes.data()
	};
	SR_VK_CHECK(vkCreateDescriptorPool(dev->device, &poolInfo, nullptr, &dev->descriptor_pool), "Create descriptor pool");

	// Descriptor set layout

	VkDescriptorType mutableDescriptorTypes[] = {
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, // CBV
		VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,  // SRV
		VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  // UAV
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER  // SRV/UAV
	};
	VkMutableDescriptorTypeListEXT mutableDescriptorTypeList = {
		.descriptorTypeCount = _countof(mutableDescriptorTypes),
		.pDescriptorTypes = mutableDescriptorTypes
	};
	VkMutableDescriptorTypeCreateInfoEXT mutableDescriptorTypeInfo = {
		.sType = VK_STRUCTURE_TYPE_MUTABLE_DESCRIPTOR_TYPE_CREATE_INFO_EXT,
		.mutableDescriptorTypeListCount = 1,
		.pMutableDescriptorTypeLists = &mutableDescriptorTypeList
	};

	VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
		.pNext = &mutableDescriptorTypeInfo,
		.bindingCount = static_cast<u32>(bindingFlags.size()),
		.pBindingFlags = bindingFlags.data()
	};
	VkDescriptorSetLayoutCreateInfo setLayoutInfo = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.pNext = &bindingFlagsInfo,
		.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
		.bindingCount = static_cast<u32>(layoutBindings.size()),
		.pBindings = layoutBindings.data()
	};
	SR_VK_CHECK(vkCreateDescriptorSetLayout(dev->device, &setLayoutInfo, nullptr, &dev->descriptor_set_layout_bindless), "Create descriptor set layout");

	// Descriptor set
	VkDescriptorSetAllocateInfo descriptorSetAllocInfo = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = dev->descriptor_pool,
		.descriptorSetCount = 1,
		.pSetLayouts = &dev->descriptor_set_layout_bindless
	};
	SR_VK_CHECK(vkAllocateDescriptorSets(dev->device, &descriptorSetAllocInfo, &dev->descriptor_set_bindless), "Allocate descriptor sets");

	// Push descriptor
	VkDescriptorSetLayoutBinding uboBinding = {
		.binding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_ALL,
		.pImmutableSamplers = nullptr
	};

	VkDescriptorSetLayoutCreateInfo pushLayoutInfo = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.pNext = nullptr,
		.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT,
		.bindingCount = 1,
		.pBindings = &uboBinding
	};
	SR_VK_CHECK(vkCreateDescriptorSetLayout(dev->device, &pushLayoutInfo, nullptr, &dev->descriptor_set_layout_push), "Create push-descriptor set layout");
}

internal void SRGFXDeviceVulkan_CreateDestructionHandler(SRGFXDeviceVulkan* dev) {
	dev->destruction_handler = new SRDestructionHandler_Vulkan(dev->device, dev->instance, dev->vma_allocator);
}

// ------------------------------ Public API ------------------------------
void SRGFXVulkan_CreateDevice(SRWindow* window, SRGFXDevice* device) {
	SRGFXDeviceVulkan* dev_vulkan = (SRGFXDeviceVulkan*)malloc(sizeof(SRGFXDeviceVulkan));
	assert(dev_vulkan);
	ZeroMemory(dev_vulkan, sizeof(*dev_vulkan));

	device->internalState = dev_vulkan;
	device->vtbl = &SRGFXDevice_Vulkan_VTable;

	dev_vulkan->window = window;
	dev_vulkan->arena_general = SRArena_Create(Gigabytes(1));
	dev_vulkan->arena_upload = SRArena_Create(Gigabytes(1));

	SRGFXDeviceVulkan_CreateInstance(dev_vulkan);
	SRGFXDeviceVulkan_CreateDebugMessenger(dev_vulkan);
	SRGFXDeviceVulkan_CreateSurface(dev_vulkan);
	SRGFXDeviceVulkan_CreateDevice(dev_vulkan);
	SRGFXDeviceVulkan_CreateMemoryAllocator(dev_vulkan);
	SRGFXDeviceVulkan_CreateCommandPools(dev_vulkan);
	SRGFXDeviceVulkan_CreateSyncObjects(dev_vulkan);
	SRGFXDeviceVulkan_CreateDescriptors(dev_vulkan);
	SRGFXDeviceVulkan_CreateDestructionHandler(dev_vulkan);
}

void SRGFXVulkan_DestroyDevice(SRGFXDevice* device) {
	auto* dev = (SRGFXDeviceVulkan*)device->internalState;
	SRDescriptorHeap_Vulkan_Destroy(dev->descriptor_heap_cbv_srv_uav);
	SRDescriptorHeap_Vulkan_Destroy(dev->descriptor_heap_sampler);
	SRArena_Destroy(dev->arena_general);
	SRArena_Destroy(dev->arena_upload);

#ifdef _DEBUG
	if (dev->is_debug_utils_available) {
		dev->destruction_handler->enqueue(dev->debug_messenger);
	}
#endif

	dev->destruction_handler->enqueue(dev->surface);

	for (u32 q = 0; q < SRQueue_COUNT; ++q) {
		for (u32 f = 0; f < SR_GFX_FRAMES_IN_FLIGHT; ++f) {
			dev->destruction_handler->enqueue(dev->cmd_pools[q][f]);
		}
	}
	dev->destruction_handler->enqueue(dev->cmd_pool_upload);

	for (u32 q = 0; q < SRQueue_COUNT; ++q) {
		dev->destruction_handler->enqueue(dev->frame_fences[q]);
	}

	for (u32 f = 0; f < SR_GFX_FRAMES_IN_FLIGHT; f++) {
		dev->destruction_handler->enqueue(dev->semaphores_image_available[f]);

	}
	for (u32 b = 0; b < SR_MAX_SWAPCHAIN_IMAGES; ++b) {
		dev->destruction_handler->enqueue(dev->semaphores_render_finished[b]);
	}
	dev->destruction_handler->enqueue(dev->acquire_fence);
	dev->destruction_handler->enqueue(dev->descriptor_pool);
	dev->destruction_handler->enqueue(dev->descriptor_set_layout_push);
	dev->destruction_handler->enqueue(dev->descriptor_set_layout_bindless);

	delete dev->destruction_handler;

	free(dev);
}

u32 SRGFXVulkan_GetFrameIndex(SRGFXDevice* device) {
	auto* dev = (SRGFXDeviceVulkan*)device->internalState;
	return dev->frame_index;
}

void SRGFXVulkan_CreateSwapchain(SRGFXDevice* device, SRSwapchainInfo info, SRSwapchain* swapchain) {
	auto* dev = (SRGFXDeviceVulkan*)device->internalState;

	SRSwapchain_Vulkan* internalSwapchain;

	if (swapchain->internalState != nullptr) {
		internalSwapchain = to_vk_internal(*swapchain);
	}
	else {
		internalSwapchain = SRArena_PushStructZero(dev->arena_general, SRSwapchain_Vulkan);
	}

	swapchain->info = info;
	swapchain->internalState = internalSwapchain;

	SRSwapchainSupportInfo supportInfo = SRVulkanHelpers::query_swapchain_support(dev->physical_device, dev->surface);
	VkSurfaceFormatKHR surfaceFormat = SRVulkanHelpers::pick_surface_format(
		to_vk_format(info.format),
		info.useHDR,
		supportInfo.surfaceFormats
	);

	// Check minimum and maximum number of supported backbuffers
	if (
		(supportInfo.capabilities.minImageCount > info.numBuffers) ||
		(supportInfo.capabilities.maxImageCount > 0 && supportInfo.capabilities.maxImageCount < info.numBuffers))
	{
		SRLOG_CRITICAL_CAT(
			SRLOG_CAT_VULKAN,
			"Swapchain does not support %d backbuffers(s). Number of backbuffers must be in range [%u, %u]",
			info.numBuffers,
			supportInfo.capabilities.minImageCount,
			supportInfo.capabilities.maxImageCount
		);
		throw std::runtime_error("VULKAN ERROR: Invalid amount of buffers");
	}

	// Set image extent
	VkExtent2D extent = {};
	if (supportInfo.capabilities.currentExtent.width != std::numeric_limits<u32>::max()) {
		extent = supportInfo.capabilities.currentExtent;
	}
	else {
		SRWindow_GetClientSize(dev->window, &extent.width, &extent.height);

		extent.width = std::clamp(
			extent.width,
			supportInfo.capabilities.minImageExtent.width,
			supportInfo.capabilities.maxImageExtent.width
		);
		extent.height = std::clamp(
			extent.height,
			supportInfo.capabilities.minImageExtent.height,
			supportInfo.capabilities.maxImageExtent.height
		);
	}
	internalSwapchain->extent = extent;

	VkSwapchainKHR old_swapchain = internalSwapchain->swapchain;
	VkSwapchainCreateInfoKHR createInfo = {
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.flags = 0, // TODO: Investigate swapchain flags
		.surface = dev->surface,
		.minImageCount = info.numBuffers,
		.imageFormat = surfaceFormat.format,
		.imageColorSpace = surfaceFormat.colorSpace,
		.imageExtent = extent,
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.preTransform = supportInfo.capabilities.currentTransform,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode = SRVulkanHelpers::pick_present_mode(info.vSync, supportInfo.presentModes),
		.clipped = VK_TRUE,
		.oldSwapchain = old_swapchain
	};

	// HACK: It turns out that swapchain recreation is an underspecified portion of the
	// Vulkan spec at the moment, and the only way to "correctly" do it is to wait idle
	// before creating a new one. Another note is that acquisition of the swapchain
	// image actually is a call that does nothing because of some vendor stuff,
	// so it's not enough to rely on the swapchain being invalidated.
	if (old_swapchain != VK_NULL_HANDLE) {
		SR_VK_CHECK(vkDeviceWaitIdle(dev->device), "Wait idle");
	}

	SR_VK_CHECK(vkCreateSwapchainKHR(dev->device, &createInfo, nullptr, &internalSwapchain->swapchain), "Swapchain creation");

	if (old_swapchain != VK_NULL_HANDLE) {
		vkDestroySwapchainKHR(dev->device, old_swapchain, nullptr);

		for (u64 i = 0; i < internalSwapchain->backbuffer_count; ++i) {
			vkDestroyImageView(dev->device, internalSwapchain->backbuffers[i].vkImageView, nullptr);
		}
	}

	// Swapchain images
	u32 numImages;
	SR_VK_CHECK(vkGetSwapchainImagesKHR(dev->device, internalSwapchain->swapchain, &numImages, nullptr), "Get swapchain images");

	std::vector<VkImage> images(numImages);
	SR_VK_CHECK(vkGetSwapchainImagesKHR(dev->device, internalSwapchain->swapchain, &numImages, images.data()), "Get swapchain images");

	// Swapchain image views
	internalSwapchain->backbuffer_count = numImages;
	for (u32 i = 0; i < numImages; ++i) {
		VkImageView imageView;
		VkImageViewCreateInfo imageViewInfo = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = images[i],
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = surfaceFormat.format,
			.components = {
				.r = VK_COMPONENT_SWIZZLE_IDENTITY,
				.g = VK_COMPONENT_SWIZZLE_IDENTITY,
				.b = VK_COMPONENT_SWIZZLE_IDENTITY,
				.a = VK_COMPONENT_SWIZZLE_IDENTITY
			},
			.subresourceRange = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1
			}
		};

		SR_VK_CHECK(vkCreateImageView(dev->device, &imageViewInfo, nullptr, &imageView), "Swapchain image view creation");

		internalSwapchain->backbuffers[i] = SRSwapchain_Vulkan::Backbuffer{
			.vkImage = images[i],
			.vkImageView = imageView,
			.hasBeenUsed = false
		};
	}
}

void SRGFXVulkan_CreatePipeline(SRGFXDevice* device, const SRPipelineInfo* info, SRPipeline* pipeline) {
	auto* dev = (SRGFXDeviceVulkan*)device->internalState;
	auto* internalPipeline = SRArena_PushStructZero(dev->arena_general, SRPipeline_Vulkan);

	pipeline->info = *info;
	pipeline->internalState = internalPipeline;

	std::vector<VkShaderModule> shaderModules;
	std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

	// TODO: Would be nice to have pipeline caching
	if (info->vertexShader != nullptr) {
		VkShaderModule shaderModule = SRVulkanHelpers::create_shader_module(dev->device, info->vertexShader);
		assert(shaderModule);
		shaderModules.push_back(shaderModule);
		
		VkPipelineShaderStageCreateInfo shaderStageInfo = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = shaderModule,
			.pName = info->vertexShader->entry_point,
			.pSpecializationInfo = nullptr
		};
		shaderStages.push_back(shaderStageInfo);
	}
	if (info->pixelShader != nullptr) {
		VkShaderModule shaderModule = SRVulkanHelpers::create_shader_module(dev->device, info->pixelShader);
		assert(shaderModule);
		shaderModules.push_back(shaderModule);

		VkPipelineShaderStageCreateInfo shaderStageInfo = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = shaderModule,
			.pName = info->pixelShader->entry_point,
			.pSpecializationInfo = nullptr
		};
		shaderStages.push_back(shaderStageInfo);
	}

	// Dynamic states
	const std::vector<VkDynamicState> dynamicStates = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	const VkPipelineDynamicStateCreateInfo dynamicStateInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = static_cast<u32>(dynamicStates.size()),
		.pDynamicStates = dynamicStates.data()
	};

	// Attribute and binding descriptions
	std::vector<VkVertexInputAttributeDescription> attributeDescriptions(info->inputLayout.num_elements);
	u32 offset = 0;

	for (size_t i = 0; i < attributeDescriptions.size(); i++) {
		attributeDescriptions[i].binding = 0;
		attributeDescriptions[i].location = static_cast<u32>(i); // TODO: Doesn't work for all formats
		attributeDescriptions[i].format = to_vk_format(info->inputLayout.elements[i].format);
		attributeDescriptions[i].offset = offset;

		offset += SRGraphicsHelpers::get_format_stride(info->inputLayout.elements[i].format);
	}

	// TODO: For now we only allow one binding description
	VkVertexInputBindingDescription bindingDescription = {
		.binding = 0,
		.stride = offset, // total offset is equivalent to stride
		.inputRate = VK_VERTEX_INPUT_RATE_VERTEX
	};
	VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = attributeDescriptions.empty() ? 0U : 1U,
		.pVertexBindingDescriptions = attributeDescriptions.empty() ? nullptr : &bindingDescription,
		.vertexAttributeDescriptionCount = static_cast<u32>(attributeDescriptions.size()),
		.pVertexAttributeDescriptions = attributeDescriptions.empty() ? nullptr : attributeDescriptions.data()
	};
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, // TODO: Support other topologies
		.primitiveRestartEnable = VK_FALSE
	};
	VkPipelineViewportStateCreateInfo viewportState = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.scissorCount = 1
	};
	VkPipelineRasterizationStateCreateInfo rasterizerInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.depthClampEnable = VK_FALSE,
		.rasterizerDiscardEnable = VK_FALSE,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.cullMode = to_vk_cull_mode(info->rasterizerState.cullMode),
		.frontFace = info->rasterizerState.frontCW ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE,
		.depthBiasEnable = VK_FALSE,
		.depthBiasConstantFactor = 0.0f,
		.depthBiasClamp = 0.0f,
		.depthBiasSlopeFactor = 0.0f,
		.lineWidth = 1.0f
	};
	VkPipelineMultisampleStateCreateInfo multisamplingInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
		.sampleShadingEnable = VK_FALSE,
		.minSampleShading = 1.0f,
		.pSampleMask = nullptr,
		.alphaToCoverageEnable = VK_FALSE,
		.alphaToOneEnable = VK_FALSE
	};

	// Blending
	std::vector<VkPipelineColorBlendAttachmentState> colorBlendStates;
	for (u32 i = 0; i < info->numRenderTargets; ++i) {
		const SRBlendState::RenderTargetBlendState* blendState = &info->blendState.renderTargetBlendStates[i];

		// TODO: Make dynamic
		VkPipelineColorBlendAttachmentState colorBlendState = {
			.blendEnable = blendState->blendEnable ? VK_TRUE : VK_FALSE,
			.srcColorBlendFactor = to_vk_blend(blendState->srcBlend),
			.dstColorBlendFactor = to_vk_blend(blendState->dstBlend),
			.colorBlendOp = to_vk_blend_op(blendState->blendOp),
			.srcAlphaBlendFactor = to_vk_blend(blendState->srcBlendAlpha),
			.dstAlphaBlendFactor = to_vk_blend(blendState->dstBlendAlpha),
			.alphaBlendOp = to_vk_blend_op(blendState->blendOpAlpha),
			.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
		};

		colorBlendStates.push_back(colorBlendState);
	}
	VkPipelineColorBlendStateCreateInfo colorBlendInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.logicOpEnable = VK_FALSE,
		.logicOp = VK_LOGIC_OP_COPY,
		.attachmentCount = static_cast<u32>(colorBlendStates.size()),
		.pAttachments = colorBlendStates.data(),
		.blendConstants = { 0.0f, 0.0f, 0.0f, 0.0f }
	};

	// Descriptors
	VkPushConstantRange pushConstantRange = {
		.stageFlags = VK_SHADER_STAGE_ALL,
		.offset = 0,
		.size = 128
	};
	VkDescriptorSetLayout setLayouts[] = {
		dev->descriptor_set_layout_bindless, // set 0
		dev->descriptor_set_layout_push // set 1
	};
	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = std::size(setLayouts),
		.pSetLayouts = setLayouts,
		.pushConstantRangeCount = 1,
		.pPushConstantRanges = &pushConstantRange
	};
	SR_VK_CHECK(vkCreatePipelineLayout(
		dev->device,
		&pipelineLayoutInfo,
		nullptr,
		&internalPipeline->pipelineLayout
	), "Create pipeline layout");

	std::vector<VkFormat> colorAttachmentFormats = {};
	colorAttachmentFormats.reserve(static_cast<size_t>(info->numRenderTargets));

	for (size_t i = 0; i < info->numRenderTargets; ++i) {
		colorAttachmentFormats.push_back(to_vk_format(info->renderTargetFormats[i]));
	}

	// TODO: Stencil format unspecified right now
	VkPipelineRenderingCreateInfo pipelineRenderingInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		.colorAttachmentCount = info->numRenderTargets,
		.pColorAttachmentFormats = colorAttachmentFormats.data(),
		.depthAttachmentFormat = to_vk_format(info->depthStencilFormat)
	};
	VkPipelineDepthStencilStateCreateInfo depthStencilInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.depthTestEnable = info->depthStencilState.depthEnable ? VK_TRUE : VK_FALSE,
		.depthWriteEnable = info->depthStencilState.depthWriteMask == SRDepthWriteMask::Zero ? VK_FALSE : VK_TRUE,
		.depthCompareOp = to_vk_comparison_func(info->depthStencilState.depthFunction),
		.depthBoundsTestEnable = VK_FALSE,
		.stencilTestEnable = VK_FALSE,
		.minDepthBounds = 0.0f,
		.maxDepthBounds = 1.0f
	};
	VkGraphicsPipelineCreateInfo pipelineInfo = {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.pNext = &pipelineRenderingInfo,
		.stageCount = static_cast<u32>(shaderStages.size()),
		.pStages = shaderStages.data(),
		.pVertexInputState = &vertexInputInfo,
		.pInputAssemblyState = &inputAssemblyInfo,
		.pViewportState = &viewportState,
		.pRasterizationState = &rasterizerInfo,
		.pMultisampleState = &multisamplingInfo,
		.pDepthStencilState = &depthStencilInfo,
		.pColorBlendState = &colorBlendInfo,
		.pDynamicState = &dynamicStateInfo,
		.layout = internalPipeline->pipelineLayout,
		.renderPass = nullptr,
		.subpass = 0,
		.basePipelineHandle = VK_NULL_HANDLE,
		.basePipelineIndex = -1
	};
	SR_VK_CHECK(vkCreateGraphicsPipelines(
		dev->device,
		nullptr,
		1,
		&pipelineInfo,
		nullptr,
		&internalPipeline->pipeline
	), "Create graphics pipeline");

	for (const auto& shaderModule : shaderModules) {
		dev->destruction_handler->enqueue(shaderModule);
	}
}

// TODO: Add support for ReBar devices
void SRGFXVulkan_CreateBuffer(SRGFXDevice* device, SRBufferInfo info, SRBuffer* buffer, const void* data) {
	auto* dev = (SRGFXDeviceVulkan*)device->internalState;
	auto* internalBuffer = SRArena_PushStructZero(dev->arena_general, SRBuffer_Vulkan);

	buffer->type = SRResourceType::Buffer;
	buffer->info = info;
	buffer->internalState = internalBuffer;
	buffer->mappedData = nullptr;
	buffer->mappedSize = 0;

	VkBufferCreateInfo createInfo = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = info.size,
		.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE
	};
	VmaAllocationCreateInfo allocCreateInfo = {
		.usage = VMA_MEMORY_USAGE_AUTO
	};

	if (has_flag(info.bindFlags, SRBindFlag::VertexBuffer)) {
		createInfo.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	}
	else if (has_flag(info.bindFlags, SRBindFlag::IndexBuffer)) {
		createInfo.usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	}
	else if (has_flag(info.bindFlags, SRBindFlag::ConstantBuffer)) {
		createInfo.usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	}
	if (has_flag(info.miscFlags, SRMiscFlag::StructuredBuffer)) {
		createInfo.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	}

	switch (info.usage) {
	case SRUsage::Upload:
		allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
		break;
	}

	VmaAllocationInfo allocInfo = {};
	SR_VK_CHECK(vmaCreateBuffer(
		dev->vma_allocator,
		&createInfo,
		&allocCreateInfo,
		&internalBuffer->buffer,
		&internalBuffer->allocation,
		&allocInfo
	), "Create buffer");

	if (info.usage == SRUsage::Default && data != nullptr) {
		// Staging buffer
		SRBufferInfo stagingBufferInfo = info;
		stagingBufferInfo.usage = SRUsage::Upload;
		stagingBufferInfo.bindFlags = SRBindFlag::None;
		stagingBufferInfo.miscFlags = SRMiscFlag::None;

		SRBuffer stagingBuffer;
		SRGFXVulkan_CreateBuffer(device, stagingBufferInfo, &stagingBuffer, data);
		auto* internal_staging_buffer = to_vk_internal(stagingBuffer);

		SRResource* upload = SRArena_PushStruct(dev->arena_upload, SRResource);
		*upload = stagingBuffer;

		// Copy staging buffer into target buffer
		if (!dev->is_upload_cmd_buffer_recording) {
			VkCommandBufferBeginInfo beginInfo = {
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
				.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
			};

			SR_VK_CHECK(vkResetCommandPool(dev->device, dev->cmd_pool_upload, 0), "Reset command pool");
			SR_VK_CHECK(vkBeginCommandBuffer(dev->cmd_buffer_upload, &beginInfo), "Begin command buffer");
			dev->is_upload_cmd_buffer_recording = true;
		}

		VkBufferCopy copyRegion = {
			.srcOffset = 0,
			.dstOffset = 0,
			.size = info.size
		};
		vkCmdCopyBuffer(
			dev->cmd_buffer_upload,
			internal_staging_buffer->buffer,
			internalBuffer->buffer,
			1,
			&copyRegion
		);
	}
	else if (info.usage == SRUsage::Upload) {
		buffer->mappedData = internalBuffer->allocation->GetMappedData();
		buffer->mappedSize = info.size;

		if (data != nullptr) {
			memcpy(buffer->mappedData, data, info.size);
		}
	}

	// TODO: Descriptors (non UBO that is)
}

void SRGFXVulkan_CreateTexture(SRGFXDevice* device, const SRTextureInfo* info, SRTexture* texture, const SRSubresourceData* data) {
	auto* dev = (SRGFXDeviceVulkan*)device->internalState;
	assert(info->usage == SRUsage::Default);

	SRTexture_Vulkan* internalTexture;

	if (texture->internalState != nullptr) {
		internalTexture = to_vk_internal(*texture);
	}
	else {
		internalTexture = SRArena_PushStructZero(dev->arena_general, SRTexture_Vulkan);
	}

	texture->type = SRResourceType::Texture;
	texture->info = *info;
	texture->internalState = internalTexture;

	VkImageCreateInfo imageInfo = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.flags = 0,
		.imageType = VK_IMAGE_TYPE_2D, // TODO: Make dynamic
		.format = to_vk_format(info->format),
		.extent = { info->width, info->height, info->depth },
		.mipLevels = info->mipLevels,
		.arrayLayers = info->arraySize,
		.samples = VK_SAMPLE_COUNT_1_BIT, // TODO: Make dynamic
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
	};
	// TODO: Not valid for transient attachments ^^
	VmaAllocationCreateInfo allocCreateInfo = {
		.usage = VMA_MEMORY_USAGE_AUTO
	};

	VkAccessFlags2 accessFlags = 0;

	if (has_flag(info->bindFlags, SRBindFlag::ShaderResource)) {
		imageInfo.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
		accessFlags |= VK_ACCESS_2_SHADER_READ_BIT;
	}
	if (has_flag(info->bindFlags, SRBindFlag::UnorderedAccess)) {
		imageInfo.usage |= VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	}

	if (has_flag(info->bindFlags, SRBindFlag::RenderTarget)) {
		imageInfo.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		accessFlags |= VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;
		accessFlags |= VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	}
	else if (has_flag(info->bindFlags, SRBindFlag::DepthStencil)) {
		imageInfo.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		accessFlags |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
		accessFlags |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	}

	VmaAllocationInfo allocInfo = {};
	SR_VK_CHECK(vmaCreateImage(
		dev->vma_allocator,
		&imageInfo,
		&allocCreateInfo,
		&internalTexture->image,
		&internalTexture->allocation,
		&allocInfo
	), "Create image");

	bool isDepthFormat = SRGraphicsHelpers::is_depth_format(info->format);
	VkImageAspectFlags aspectMask = isDepthFormat ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
	VkImageViewCreateInfo imageViewInfo = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = internalTexture->image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D, // TODO: Make dynamic
		.format = to_vk_format(info->format),
		.components = {
			.r = VK_COMPONENT_SWIZZLE_IDENTITY,
			.g = VK_COMPONENT_SWIZZLE_IDENTITY,
			.b = VK_COMPONENT_SWIZZLE_IDENTITY,
			.a = VK_COMPONENT_SWIZZLE_IDENTITY,
		},
		.subresourceRange = {
			.aspectMask = aspectMask,
			.baseMipLevel = 0,
			.levelCount = info->mipLevels,
			.baseArrayLayer = 0,
			.layerCount = info->arraySize
		}
	};
	SR_VK_CHECK(vkCreateImageView(
		dev->device,
		&imageViewInfo,
		nullptr,
		&internalTexture->imageView
	), "Create image view");

	if (data && data->data) {
		// Staging buffer
		SRBufferInfo stagingBufferInfo = {
			.size = static_cast<u64>(data->rowPitch * info->height),
			.usage = SRUsage::Upload
		};

		SRBuffer stagingBuffer;
		SRGFXVulkan_CreateBuffer(device, stagingBufferInfo, &stagingBuffer, data);
		auto* internalStagingBuffer = to_vk_internal(stagingBuffer);

		// Copy staging buffer into target buffer
		if (!dev->is_upload_cmd_buffer_recording) {
			const VkCommandBufferBeginInfo beginInfo = {
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
				.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
			};

			SR_VK_CHECK(vkResetCommandPool(dev->device, dev->cmd_pool_upload, 0), "Reset command pool");
			SR_VK_CHECK(vkBeginCommandBuffer(dev->cmd_buffer_upload, &beginInfo), "Begin command buffer");
			dev->is_upload_cmd_buffer_recording = true;
		}

		std::vector<VkBufferImageCopy> copyRegions;
		VkDeviceSize copyOffset = 0;
		u32 dataIdx = 0;

		for (u32 layer = 0; layer < info->arraySize; ++layer) {
			u32 width = info->width;
			u32 height = info->height;
			u32 depth = info->depth;

			for (u32 mip = 0; mip < info->mipLevels; ++mip) {
				SRSubresourceData subresourceData = data[dataIdx++];
				u32 texelBlockSize = 1; // TODO: For block-compressed textures, this must be 4, please fix
				u32 numTexelBlocksX = std::max(1U, width / texelBlockSize);
				u32 numTexelBlocksY = std::max(1U, height / texelBlockSize);
				u32 dstRowPitch = numTexelBlocksX * SRGraphicsHelpers::get_format_stride(info->format);
				u32 dstSlicePitch = dstRowPitch * numTexelBlocksY;
				u32 srcRowPitch = subresourceData.rowPitch;
				u32 srcSlicePitch = subresourceData.slicePitch;

				VkBufferImageCopy copyRegion = {
					.bufferOffset = copyOffset,
					.bufferRowLength = 0,
					.bufferImageHeight = 0,
					.imageSubresource = {
						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
						.mipLevel = mip,
						.baseArrayLayer = layer,
						.layerCount = 1
					},
					.imageOffset = { 0, 0, 0 },
					.imageExtent = { width, height, depth }
				};
				copyRegions.push_back(copyRegion);

				// NOTE: In the case of using the transfer queue (copy queue),
				// it is required that bufferOffset is a multiple of 4. So we
				// will always align to 4 bytes.
				copyOffset += dstSlicePitch * depth;
				copyOffset = align_to(copyOffset, static_cast<VkDeviceSize>(4));

				width = std::max(1U, width / 2);
				height = std::max(1U, height / 2);
				depth = std::max(1U, depth / 2);
			}
		}

		// Transition image to be CopyDst
		// TODO: Use the GLOBAL image transition interface instead (i.e. barrier())
		SRImageTransitionInfo transitionInfo = {
			.image = internalTexture->image,
			.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			.srcAccessMask = VK_ACCESS_2_NONE,
			.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
			.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
			.aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT
		};
		SRVulkanHelpers::transition_image_layout(transitionInfo, dev->cmd_buffer_upload);

		vkCmdCopyBufferToImage(
			dev->cmd_buffer_upload,
			internalStagingBuffer->buffer,
			internalTexture->image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			static_cast<u32>(copyRegions.size()),
			copyRegions.data()
		);
	}

	// TODO: More descriptors
	// SRV Descriptor
	// TODO: Cleanup, move descriptor write functions into separate file
	if (has_flag(info->bindFlags, SRBindFlag::ShaderResource)) {
		VkDescriptorImageInfo descriptorImageInfo = {
			.sampler = nullptr,
			.imageView = internalTexture->imageView,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};

		internalTexture->srvDescriptor = SRDescriptorHeap_Vulkan_GetNextIndex(dev->descriptor_heap_cbv_srv_uav);

		VkWriteDescriptorSet descriptorWrite = {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = dev->descriptor_set_bindless,
			.dstBinding = 0,
			.dstArrayElement = internalTexture->srvDescriptor,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
			.pImageInfo = &descriptorImageInfo
		};

		vkUpdateDescriptorSets(dev->device, 1, &descriptorWrite, 0, nullptr);
	}
}

void SRGFXVulkan_CreateSampler(SRGFXDevice* device, SRSamplerInfo info, SRSampler* sampler) {
	auto* dev = (SRGFXDeviceVulkan*)device->internalState;
	auto* internalSampler = SRArena_PushStructZero(dev->arena_general, SRSampler_Vulkan);

	sampler->type = SRResourceType::Sampler;
	sampler->info = info;
	sampler->internalState = internalSampler;

	VkSamplerCreateInfo samplerCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.addressModeU = to_vk_texture_address_mode(info.addressU),
		.addressModeV = to_vk_texture_address_mode(info.addressV),
		.addressModeW = to_vk_texture_address_mode(info.addressW),
		.mipLodBias = info.mipLODBias,
		.anisotropyEnable = VK_TRUE,
		.maxAnisotropy = 16, // TODO: Enforce or check if always available
		.compareOp = to_vk_comparison_func(info.comparisonFunc),
		.minLod = info.minLOD,
		.maxLod = std::numeric_limits<float>::max(), // TODO: Please fix
		.borderColor = to_vk_sampler_border_color(info.borderColor),
		.unnormalizedCoordinates = VK_FALSE
	};

	switch (info.filter) {
	case SRFilter::MinMagMipPoint:
	case SRFilter::MinimumMinMagMipPoint:
	case SRFilter::MaximumMinMagMipPoint:
		samplerCreateInfo.minFilter = VK_FILTER_NEAREST;
		samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
		samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		samplerCreateInfo.anisotropyEnable = VK_FALSE;
		samplerCreateInfo.compareEnable = VK_FALSE;
		break;
	case SRFilter::MinMagPointMipLinear:
	case SRFilter::MinimumMinMagPointMipLinear:
	case SRFilter::MaximumMinMagPointMipLinear:
		samplerCreateInfo.minFilter = VK_FILTER_NEAREST;
		samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
		samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCreateInfo.anisotropyEnable = VK_FALSE;
		samplerCreateInfo.compareEnable = VK_FALSE;
		break;
	case SRFilter::MinPointMagLinearMipPoint:
	case SRFilter::MinimumMinPointMagLinearMipPoint:
	case SRFilter::MaximumMinPointMagLinearMipPoint:
		samplerCreateInfo.minFilter = VK_FILTER_NEAREST;
		samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
		samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		samplerCreateInfo.anisotropyEnable = VK_FALSE;
		samplerCreateInfo.compareEnable = VK_FALSE;
		break;
	case SRFilter::MinPointMagMipLinear:
	case SRFilter::MinimumMinPointMagMipLinear:
	case SRFilter::MaximumMinPointMagMipLinear:
		samplerCreateInfo.minFilter = VK_FILTER_NEAREST;
		samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
		samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCreateInfo.anisotropyEnable = VK_FALSE;
		samplerCreateInfo.compareEnable = VK_FALSE;
		break;
	case SRFilter::MinLinearMagMipPoint:
	case SRFilter::MinimumMinLinearMagMipPoint:
	case SRFilter::MaximumMinLinearMagMipPoint:
		samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
		samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
		samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		samplerCreateInfo.anisotropyEnable = VK_FALSE;
		samplerCreateInfo.compareEnable = VK_FALSE;
		break;
	case SRFilter::MinLinearMagPointMipLinear:
	case SRFilter::MinimumMinLinearMagPointMipLinear:
	case SRFilter::MaximumMinLinearMagPointMipLinear:
		samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
		samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
		samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCreateInfo.anisotropyEnable = VK_FALSE;
		samplerCreateInfo.compareEnable = VK_FALSE;
		break;
	case SRFilter::MinMagLinearMipPoint:
	case SRFilter::MinimumMinMagLinearMipPoint:
	case SRFilter::MaximumMinMagLinearMipPoint:
		samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
		samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
		samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		samplerCreateInfo.anisotropyEnable = VK_FALSE;
		samplerCreateInfo.compareEnable = VK_FALSE;
		break;
	case SRFilter::MinMagMipLinear:
	case SRFilter::MinimumMinMagMipLinear:
	case SRFilter::MaximumMinMagMipLinear:
		samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
		samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
		samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCreateInfo.anisotropyEnable = VK_FALSE;
		samplerCreateInfo.compareEnable = VK_FALSE;
		break;
	case SRFilter::Anisotropic:
	case SRFilter::MinimumAnisotropic:
	case SRFilter::MaximumAnisotropic:
		samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
		samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
		samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCreateInfo.anisotropyEnable = VK_TRUE;
		samplerCreateInfo.maxAnisotropy = std::min(16.0f, std::max(1.0f, static_cast<float>(info.maxAnisotropy)));
		samplerCreateInfo.compareEnable = VK_FALSE;
		break;
	case SRFilter::ComparisonMinMagMipPoint:
		samplerCreateInfo.minFilter = VK_FILTER_NEAREST;
		samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
		samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		samplerCreateInfo.anisotropyEnable = VK_FALSE;
		samplerCreateInfo.compareEnable = VK_TRUE;
		break;
	case SRFilter::ComparisonMinMagPointMipLinear:
		samplerCreateInfo.minFilter = VK_FILTER_NEAREST;
		samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
		samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCreateInfo.anisotropyEnable = VK_FALSE;
		samplerCreateInfo.compareEnable = VK_TRUE;
		break;
	case SRFilter::ComparisonMinPointMagLinearMipPoint:
		samplerCreateInfo.minFilter = VK_FILTER_NEAREST;
		samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
		samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		samplerCreateInfo.anisotropyEnable = VK_FALSE;
		samplerCreateInfo.compareEnable = VK_TRUE;
		break;
	case SRFilter::ComparisonMinPointMagMipLinear:
		samplerCreateInfo.minFilter = VK_FILTER_NEAREST;
		samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
		samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		samplerCreateInfo.anisotropyEnable = VK_FALSE;
		samplerCreateInfo.compareEnable = VK_TRUE;
		break;
	case SRFilter::ComparisonMinLinearMagMipPoint:
		samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
		samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
		samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		samplerCreateInfo.anisotropyEnable = VK_FALSE;
		samplerCreateInfo.compareEnable = VK_TRUE;
		break;
	case SRFilter::ComparisonMinLinearMagPointMipLinear:
		samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
		samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
		samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCreateInfo.anisotropyEnable = VK_FALSE;
		samplerCreateInfo.compareEnable = VK_TRUE;
		break;
	case SRFilter::ComparisonMinMagLinearMipPoint:
		samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
		samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
		samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		samplerCreateInfo.anisotropyEnable = VK_FALSE;
		samplerCreateInfo.compareEnable = VK_TRUE;
		break;
	case SRFilter::ComparisonMinMagMipLinear:
		samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
		samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
		samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCreateInfo.anisotropyEnable = VK_FALSE;
		samplerCreateInfo.compareEnable = VK_TRUE;
		break;
	case SRFilter::ComparisonAnisotropic:
		samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
		samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
		samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCreateInfo.anisotropyEnable = VK_TRUE;
		samplerCreateInfo.compareEnable = VK_TRUE;
		break;
	default:
		samplerCreateInfo.minFilter = VK_FILTER_NEAREST;
		samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
		samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		samplerCreateInfo.anisotropyEnable = VK_FALSE;
		samplerCreateInfo.compareEnable = VK_FALSE;
		break;
	}

	SR_VK_CHECK(vkCreateSampler(dev->device, &samplerCreateInfo, nullptr, &internalSampler->sampler), "Create sampler");

	// Create sampler descriptor
	// TODO: Move into GraphicsHelpers_Vulkan for cleanup purposes
	VkDescriptorImageInfo imageInfo = {
		.sampler = internalSampler->sampler
	};

	VkWriteDescriptorSet write = {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = dev->descriptor_set_bindless,
		.dstBinding = 1,
		.dstArrayElement = 0,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
		.pImageInfo = &imageInfo
	};

	vkUpdateDescriptorSets(
		dev->device,
		1,
		&write,
		0,
		nullptr
	);
}

void SRGFXVulkan_DestroySwapchain(SRGFXDevice* device, SRSwapchain* swapchain) {
	auto* dev = (SRGFXDeviceVulkan*)device->internalState;
	auto* internal_swapchain = to_vk_internal(*swapchain);

	dev->destruction_handler->enqueue(internal_swapchain->swapchain);

	for (size_t i = 0; i < internal_swapchain->backbuffer_count; i++) {
		dev->destruction_handler->enqueue(internal_swapchain->backbuffers[i].vkImageView);
	}

	swapchain->internalState = nullptr;
}

void SRGFXVulkan_DestroyPipeline(SRGFXDevice* device, SRPipeline* pipeline) {
	auto* dev = (SRGFXDeviceVulkan*)device->internalState;
	auto* internal_pipeline = to_vk_internal(*pipeline);

	dev->destruction_handler->enqueue(internal_pipeline->pipeline);
	dev->destruction_handler->enqueue(internal_pipeline->pipelineLayout);

	pipeline->internalState = nullptr;
}

void SRGFXVulkan_DestroyResource(SRGFXDevice* device, SRResource* resource) {
	auto* dev = (SRGFXDeviceVulkan*)device->internalState;

	// TODO: Remove sampler as resource type
	switch (resource->type) {
	case SRResourceType::Buffer:
		{
			auto* internal_buffer = (SRBuffer_Vulkan*)resource->internalState;
			dev->destruction_handler->enqueue(internal_buffer->buffer);
			dev->destruction_handler->enqueue(internal_buffer->allocation);

			ZeroMemory(internal_buffer, sizeof(*internal_buffer));
		}
		break;
	case SRResourceType::Texture:
		{
			auto* internal_texture = (SRTexture_Vulkan*)resource->internalState;
			dev->destruction_handler->enqueue(internal_texture->image, internal_texture->allocation);
			dev->destruction_handler->enqueue(internal_texture->imageView);

			if (internal_texture->srvDescriptor != SR_INVALID_DESCRIPTOR_INDEX) {
				SRDescriptorHeap_Vulkan_FreeIndex(dev->descriptor_heap_cbv_srv_uav, internal_texture->srvDescriptor);
			}

			ZeroMemory(internal_texture, sizeof(*internal_texture));
		}
		break;
	case SRResourceType::Sampler:
		{
			auto* internal_sampler = (SRSampler_Vulkan*)resource->internalState;
			dev->destruction_handler->enqueue(internal_sampler->sampler);

			if (internal_sampler->samplerDescriptor != SR_INVALID_DESCRIPTOR_INDEX) {
				SRDescriptorHeap_Vulkan_FreeIndex(dev->descriptor_heap_sampler, internal_sampler->samplerDescriptor);
			}

			ZeroMemory(internal_sampler, sizeof(*internal_sampler));
		}
		break;
	}
}

void SRGFXVulkan_BindPipeline(SRGFXDevice* device, const SRPipeline* pipeline, SRCmdList cmdList) {
	auto* dev = (SRGFXDeviceVulkan*)device->internalState;
	auto* internalPipeline = to_vk_internal(*pipeline);
	auto* internalCmdList = to_vk_internal(cmdList);

	vkCmdBindPipeline(internalCmdList->cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, internalPipeline->pipeline);
	dev->active_pipeline = internalPipeline;

	vkCmdBindDescriptorSets(
		internalCmdList->cmdBuffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		internalPipeline->pipelineLayout,
		0,
		1,
		&dev->descriptor_set_bindless,
		0,
		nullptr
	);
}

void SRGFXVulkan_BindViewport(SRGFXDevice* device, const SRViewport* viewport, SRCmdList cmdList) {
	auto* internalCmdList = to_vk_internal(cmdList);

	// NOTE: We need to flip the viewport vertically in order to work with DX12
	VkViewport vkViewport = {
		.x = viewport->topLeftX,
		.y = viewport->topLeftY + viewport->height,
		.width = viewport->width,
		.height = -viewport->height,
		.minDepth = viewport->minDepth,
		.maxDepth = viewport->maxDepth
	};
	VkRect2D scissor = {
		.offset = { 0, 0 },
		.extent = {
			.width = static_cast<u32>(viewport->width),
			.height = static_cast<u32>(viewport->height)
		}
	};

	vkCmdSetViewport(internalCmdList->cmdBuffer, 0, 1, &vkViewport);
	vkCmdSetScissor(internalCmdList->cmdBuffer, 0, 1, &scissor);
}

void SRGFXVulkan_BindVertexBuffer(SRGFXDevice* device, const SRBuffer* buffer, SRCmdList cmdList) {
	assert(has_flag(buffer->info.bindFlags, SRBindFlag::VertexBuffer));
	auto* dev = (SRGFXDeviceVulkan*)device->internalState;
	auto* internalBuffer = to_vk_internal(*buffer);
	auto* internalCmdList = to_vk_internal(cmdList);

	VkDeviceSize offset = 0;
	vkCmdBindVertexBuffers(internalCmdList->cmdBuffer, 0, 1, &internalBuffer->buffer, &offset);
}

void SRGFXVulkan_BindIndexBuffer(SRGFXDevice* device, const SRBuffer* buffer, SRCmdList cmdList) {
	assert(has_flag(buffer->info.bindFlags, SRBindFlag::IndexBuffer));
	auto* dev = (SRGFXDeviceVulkan*)device->internalState;
	auto* internalBuffer = to_vk_internal(*buffer);
	auto* internalCmdList = to_vk_internal(cmdList);

	vkCmdBindIndexBuffer(internalCmdList->cmdBuffer, internalBuffer->buffer, 0, VK_INDEX_TYPE_UINT32);
}

void SRGFXVulkan_BindRootConstantBuffer(SRGFXDevice* device, const SRBuffer* buffer, SRCmdList cmdList) {
	auto* dev = (SRGFXDeviceVulkan*)device->internalState;

	assert(has_flag(buffer->info.bindFlags, SRBindFlag::ConstantBuffer));
	assert(dev->active_pipeline != nullptr);

	auto* internalBuffer = to_vk_internal(*buffer);
	auto* internalCmdList = to_vk_internal(cmdList);

	VkDescriptorBufferInfo bufferInfo = {
		.buffer = internalBuffer->buffer,
		.offset = 0,
		.range = buffer->info.size
	};
	VkWriteDescriptorSet writeDescriptor = {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = VK_NULL_HANDLE,
		.dstBinding = 0,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.pBufferInfo = &bufferInfo,
	};

	vkCmdPushDescriptorSet(
		internalCmdList->cmdBuffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		dev->active_pipeline->pipelineLayout,
		1, // set 1
		1,
		&writeDescriptor
	);
}

void SRGFXVulkan_PushConstants(SRGFXDevice* device, const void* data, u32 size, SRCmdList cmdList) {
	auto* dev = (SRGFXDeviceVulkan*)device->internalState;

	assert(data != nullptr);
	assert(size <= 128);
	assert(dev->active_pipeline != nullptr);

	auto internalCmdList = to_vk_internal(cmdList);

	vkCmdPushConstants(
		internalCmdList->cmdBuffer,
		dev->active_pipeline->pipelineLayout,
		VK_SHADER_STAGE_ALL,
		0,
		size,
		data
	);
}

void SRGFXVulkan_Barrier(SRGFXDevice* device, const SRBarrier* barriers, u32 numBarriers, SRCmdList cmdList) {
	if (!barriers || numBarriers == 0) {
		return;
	}

	auto* internalCmdList = to_vk_internal(cmdList);
	std::vector<VkImageMemoryBarrier2> vkBarriers;
	vkBarriers.reserve(numBarriers);

	// TODO: Allow for UAV and buffer barriers, not only image barriers
	for (u32 i = 0; i < numBarriers; ++i) {
		const SRBarrier* barrier = &barriers[i];
		bool isDepthFormat = SRGraphicsHelpers::is_depth_format(barrier->image.texture->info.format);
		auto* internalTexture = to_vk_internal(*barrier->image.texture);

		VkImageAspectFlags aspectFlag = isDepthFormat ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
		VkImageSubresourceRange subresourceRange = {
			.aspectMask = aspectFlag,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1
		};

		VkImageMemoryBarrier2 imageBarrier = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.pNext = nullptr,
			.srcStageMask = to_vk_pipeline_stage(barrier->image.syncBefore),
			.srcAccessMask = to_vk_access_mask(barrier->image.accessBefore),
			.dstStageMask = to_vk_pipeline_stage(barrier->image.syncAfter),
			.dstAccessMask = to_vk_access_mask(barrier->image.accessAfter),
			.oldLayout = to_vk_resource_state(barrier->image.stateBefore),
			.newLayout = to_vk_resource_state(barrier->image.stateAfter),
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = internalTexture->image,
			.subresourceRange = subresourceRange // TODO: Fix barrier subresource range to work for multiple mips if requested
		};

		vkBarriers.push_back(imageBarrier);
	}

	// TODO: Doesn't work for multiple mips
	VkDependencyInfo dependencyInfo = {
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.pNext = nullptr,
		.imageMemoryBarrierCount = static_cast<u32>(vkBarriers.size()),
		.pImageMemoryBarriers = vkBarriers.data()
	};
	vkCmdPipelineBarrier2(internalCmdList->cmdBuffer, &dependencyInfo);
}

void SRGFXVulkan_BeginFrame(SRGFXDevice* device, const SRSwapchain* swapchain) {
	auto* dev = (SRGFXDeviceVulkan*)device->internalState;

	if (dev->frame_counter >= SR_GFX_FRAMES_IN_FLIGHT) {
		u64 needed = dev->frame_done_values[SRQueue_Universal][dev->frame_index];

		VkSemaphoreWaitInfo waitInfo = {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
			.semaphoreCount = 1,
			.pSemaphores = &dev->frame_fences[SRQueue_Universal],
			.pValues = &needed
		};

		SR_VK_CHECK(vkWaitSemaphores(dev->device, &waitInfo, UINT64_MAX), "Wait for semaphore");
	}
	dev->destruction_handler->update(dev->frame_counter, SR_GFX_FRAMES_IN_FLIGHT);

	auto* internalSwapchain = to_vk_internal(*swapchain);

	// NOTE: Swapchain image acquisition is an underspecified part of the Vulkan spec.
	// vkAcquireNextImageKHR returns presentable image index immediately, but the
	// presentation engine may not have finished reading from the image.
	// The Vulkan spec states that:
	// "the application must use semaphore and/or fence to ensure that the image layout and
	// contents are not modified until the presentation engine reads have completed"
	//
	// Although the spec says "semaphores AND/OR fence", both are actually required.
	// An acquire-semaphore for GPU-GPU sync, and an acquire-FENCE for CPU-CPU sync.
	// The fence will be signaled when the acquire is complete, meaning that we can safely continue
	// on CPU-side. Skipping the fence can result in subtle frame-pacing bugs.
	SR_VK_CHECK(vkResetFences(dev->device, 1, &dev->acquire_fence), "Reset fence");
	SR_VK_CHECK(vkAcquireNextImageKHR(
		dev->device,
		internalSwapchain->swapchain,
		UINT64_MAX,
		dev->semaphores_image_available[dev->frame_index],
		dev->acquire_fence,
		&dev->image_index
	), "Acquire next swapchain image");
	SR_VK_CHECK(vkWaitForFences(dev->device, 1, &dev->acquire_fence, VK_TRUE, UINT64_MAX), "Wait for fence");
}

SRCmdList SRGFXVulkan_BeginCommandList(SRGFXDevice* device, SRQueue queue) {
	auto* dev = (SRGFXDeviceVulkan*)device->internalState;
	auto* internal_cmd_list = &dev->cmd_lists[queue][dev->frame_index];

	if (internal_cmd_list->cmdBuffer == VK_NULL_HANDLE) {
		VkCommandBufferAllocateInfo allocInfo = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool = dev->cmd_pools[queue][dev->frame_index],
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = 1
		};

		SR_VK_CHECK(vkAllocateCommandBuffers(dev->device, &allocInfo, &internal_cmd_list->cmdBuffer), "Command buffer creation");
	}

	VkCommandBufferBeginInfo beginInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};

	// Reset the command pool JUST BEFORE we begin command buffer recording.
	// This results in as little potential CPU waiting as possible.
	// Should only be done ONCE per frame per queue family.
	SR_VK_CHECK(vkResetCommandPool(dev->device, dev->cmd_pools[queue][dev->frame_index], 0), "Reset command pool");
	SR_VK_CHECK(vkBeginCommandBuffer(internal_cmd_list->cmdBuffer, &beginInfo), "Begin command buffer recording");

	return SRCmdList{ internal_cmd_list };
}

void SRGFXVulkan_BeginRenderPassSwapchain(SRGFXDevice* device, const SRSwapchain* swapchain, SRCmdList cmdList) {
	auto* dev = (SRGFXDeviceVulkan*)device->internalState;
	auto* internalSwapchain = to_vk_internal(*swapchain);
	auto* internalCmdList = to_vk_internal(cmdList);

	SRSwapchain_Vulkan::Backbuffer* currBackbuffer = &internalSwapchain->backbuffers[dev->image_index];
	SRImageTransitionInfo transitionInfo = {
		.image = currBackbuffer->vkImage,
		.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.srcAccessMask = VK_ACCESS_2_NONE,
		.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
		.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		.aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT
	};

	// NOTE: This is dumb, but technically required by the Vulkan spec
	if (!currBackbuffer->hasBeenUsed) {
		transitionInfo.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		currBackbuffer->hasBeenUsed = true;
	}
	SRVulkanHelpers::transition_image_layout(transitionInfo, internalCmdList->cmdBuffer);

	VkClearValue clearColor = {
		.color = { 0.0f, 0.0f, 0.0f, 1.0f }
	};

	VkRenderingAttachmentInfo colorAttachmentInfo = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
		.imageView = currBackbuffer->vkImageView,
		.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.clearValue = clearColor
	};

	// TODO: Depth attachment
	VkRect2D area{
		.offset = { 0, 0 },
		.extent = internalSwapchain->extent
	};

	VkRenderingInfo renderInfo = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
		.renderArea = area,
		.layerCount = 1,
		.viewMask = 0,
		.colorAttachmentCount = 1,
		.pColorAttachments = &colorAttachmentInfo,
		.pDepthAttachment = nullptr,
		.pStencilAttachment = nullptr
	};

	vkCmdBeginRendering(internalCmdList->cmdBuffer, &renderInfo);
}

void SRGFXVulkan_BeginRenderPass(SRGFXDevice* device, const SRPassInfo* passInfo, SRCmdList cmdList) {
	auto* internalCmdList = to_vk_internal(cmdList);

	std::vector<VkRenderingAttachmentInfo> colorAttachmentInfos;
	VkRenderingAttachmentInfo depthAttachmentInfo;
	colorAttachmentInfos.reserve(passInfo->numColorAttachments);

	VkRect2D renderArea = {
		.offset = { 0, 0 },
		.extent = { 0, 0 }
	};

	for (size_t i = 0; i < passInfo->numColorAttachments; ++i) {
		const SRPassInfo::Attachment* attachment = &passInfo->colorAttachments[i];
		auto internalTexture = to_vk_internal(*attachment->texture);
		assert(internalTexture);

		renderArea.extent.width = std::max(renderArea.extent.width, attachment->texture->info.width);
		renderArea.extent.height = std::max(renderArea.extent.height, attachment->texture->info.height);

		VkRenderingAttachmentInfo attachmentInfo = {
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = internalTexture->imageView,
			.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.loadOp = to_vk_load_op(attachment->loadOp),
			.storeOp = to_vk_store_op(attachment->storeOp),
			.clearValue = { .color = { 0.0f, 0.0f, 0.0f, 1.0f } }
		};
		colorAttachmentInfos.push_back(attachmentInfo);
	}

	bool hasDepthAttachment = passInfo->depthAttachment.texture != nullptr;
	if (hasDepthAttachment) {
		const SRPassInfo::Attachment* depthAttachment = &passInfo->depthAttachment;
		auto* internalTexture = to_vk_internal(*depthAttachment->texture);
		assert(internalTexture);

		renderArea.extent.width = std::max(renderArea.extent.width, depthAttachment->texture->info.width);
		renderArea.extent.height = std::max(renderArea.extent.height, depthAttachment->texture->info.height);

		// TODO JACK: The problem here is that for read-only depth, the image layout of VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL is incorrect
		// Thank me later ;)
		// TODO: This is perhaps not ideal, but we assume that it's read-only depth input if the
		// store-op is SRStoreOp::None
		bool isReadOnlyDepth = depthAttachment->storeOp == SRStoreOp::None;

		depthAttachmentInfo = {
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = internalTexture->imageView,
			.imageLayout = isReadOnlyDepth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			.loadOp = to_vk_load_op(depthAttachment->loadOp),
			.storeOp = to_vk_store_op(depthAttachment->storeOp),
			.clearValue = { .depthStencil = { depthAttachment->clearValue } }
		};
	}

	VkRenderingInfo renderInfo = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
		.renderArea = renderArea,
		.layerCount = 1,
		.viewMask = 0,
		.colorAttachmentCount = static_cast<u32>(colorAttachmentInfos.size()),
		.pColorAttachments = colorAttachmentInfos.data(),
		.pDepthAttachment = hasDepthAttachment ? &depthAttachmentInfo : nullptr,
		.pStencilAttachment = nullptr
	};

	vkCmdBeginRendering(internalCmdList->cmdBuffer, &renderInfo);
}

void SRGFXVulkan_EndRenderPassSwapchain(SRGFXDevice* device, const SRSwapchain* swapchain, SRCmdList cmdList) {
	auto* dev = (SRGFXDeviceVulkan*)device->internalState;
	auto* internalSwapchain = to_vk_internal(*swapchain);
	auto* internalCmdList = to_vk_internal(cmdList);

	vkCmdEndRendering(internalCmdList->cmdBuffer);

	const SRImageTransitionInfo transitionInfo = {
		.image = internalSwapchain->backbuffers[dev->image_index].vkImage,
		.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
		.dstAccessMask = VK_ACCESS_2_NONE,
		.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_2_NONE,
		.aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT
	};
	SRVulkanHelpers::transition_image_layout(transitionInfo, internalCmdList->cmdBuffer);
}

void SRGFXVulkan_EndRenderPass(SRGFXDevice* device, SRCmdList cmdList) {
	auto internalCmdList = to_vk_internal(cmdList);
	vkCmdEndRendering(internalCmdList->cmdBuffer);
}

void SRGFXVulkan_SubmitCommandLists(SRGFXDevice* device, const SRSwapchain* swapchain) {
	auto* dev = (SRGFXDeviceVulkan*)device->internalState;
	auto* internalSwapchain = to_vk_internal(*swapchain);

	// TODO: Tidy the command buffer submission for different queues to sync.
	// For now we only care about the universal queue
	SRCmdList_Vulkan* cmd_list = &dev->cmd_lists[SRQueue_Universal][dev->frame_index];
	SR_VK_CHECK(vkEndCommandBuffer(cmd_list->cmdBuffer), "End command buffer recording");

	VkCommandBufferSubmitInfo cmd_buffer_submit_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
		.commandBuffer = cmd_list->cmdBuffer,
		.deviceMask = 0
	};

	VkSemaphoreSubmitInfo waitSemaphoreInfo = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
		.semaphore = dev->semaphores_image_available[dev->frame_index],
		.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		.deviceIndex = 0
	};
	VkSemaphoreSubmitInfo fenceSignalSemaphoreInfo = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
		.semaphore = dev->frame_fences[SRQueue_Universal],
		.value = dev->frame_counter + 1,
		.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
		.deviceIndex = 0
	};
	VkSemaphoreSubmitInfo renderFinishedSignalSemaphoreInfo = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
		.semaphore = dev->semaphores_render_finished[dev->image_index],
		.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
		.deviceIndex = 0
	};
	std::vector<VkSemaphoreSubmitInfo> signalSemaphores = {
		fenceSignalSemaphoreInfo,
		renderFinishedSignalSemaphoreInfo
	};

	VkSubmitInfo2 submitInfo = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
		.waitSemaphoreInfoCount = 1,
		.pWaitSemaphoreInfos = &waitSemaphoreInfo,
		.commandBufferInfoCount = 1,
		.pCommandBufferInfos = &cmd_buffer_submit_info,
		.signalSemaphoreInfoCount = static_cast<u32>(signalSemaphores.size()),
		.pSignalSemaphoreInfos = signalSemaphores.data()
	};
	SR_VK_CHECK(vkQueueSubmit2(dev->cmd_queues[SRQueue_Universal], 1, &submitInfo, nullptr), "Queue submission");

	VkPresentInfoKHR presentInfo = {
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &dev->semaphores_render_finished[dev->image_index],
		.swapchainCount = 1,
		.pSwapchains = &internalSwapchain->swapchain,
		.pImageIndices = &dev->image_index
	};

	SR_VK_CHECK(vkQueuePresentKHR(dev->cmd_queues[SRQueue_Universal], &presentInfo), "Swapchain present");

	// Await frame value
	dev->frame_done_values[SRQueue_Universal][dev->frame_index] = dev->frame_counter + 1;
	dev->frame_index = (dev->frame_index + 1) % SR_GFX_FRAMES_IN_FLIGHT;
	++dev->frame_counter;
}

void SRGFXVulkan_Draw(SRGFXDevice* device, u32 vtxCount, u32 startVtx, SRCmdList cmdList) {
	auto* internalCmdList = to_vk_internal(cmdList);
	vkCmdDraw(internalCmdList->cmdBuffer, vtxCount, 1, startVtx, 0);
}

void SRGFXVulkan_DrawIndexed(SRGFXDevice* device, u32 idxCount, u32 startIdx, u32 baseVtx, SRCmdList cmdList) {
	auto* internalCmdList = to_vk_internal(cmdList);
	vkCmdDrawIndexed(internalCmdList->cmdBuffer, idxCount, 1, startIdx, baseVtx, 0);
}

void SRGFXVulkan_DrawInstanced(SRGFXDevice* device, u32 vtx_count, u32 inst_count, u32 start_vtx, uint32_t start_inst, SRCmdList cmd_list) {
	auto* internal_cmd_list = to_vk_internal(cmd_list);
	vkCmdDraw(internal_cmd_list->cmdBuffer, vtx_count, inst_count, start_vtx, start_inst);
}

void SRGFXVulkan_DispatchMesh(SRGFXDevice* device, u32 x, u32 y, u32 z, SRCmdList cmdList) {
	// TODO: IMPLEMENT
	assert(false);
}

SRDescriptorIndex SRGFXVulkan_GetDescriptorIndexSRV(SRGFXDevice* device, SRResource resource) {
	assert(resource.type == SRResourceType::Texture); // TODO: Support other SRV types

	if (resource.type == SRResourceType::Texture) {
		auto* internalTexture = (SRTexture_Vulkan*)resource.internalState;
		return internalTexture->srvDescriptor;
	}
	assert(false);

	return SR_INVALID_DESCRIPTOR_INDEX;
}

SRShaderCompileTarget SRGFXVulkan_GetShaderCompileTarget(SRGFXDevice* device) {
	return SRShaderCompileTarget::SPIRV;
}

void SRGFXVulkan_WaitForGPU(SRGFXDevice* device) {
	auto* dev = (SRGFXDeviceVulkan*)device->internalState;
	vkDeviceWaitIdle(dev->device);
}

void SRGFXVulkan_FlushInitialUploads(SRGFXDevice* device) {
	auto* dev = (SRGFXDeviceVulkan*)device->internalState;
	vkEndCommandBuffer(dev->cmd_buffer_upload);

	VkSubmitInfo submitInfo = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount = 1,
		.pCommandBuffers = &dev->cmd_buffer_upload
	};

	vkQueueSubmit(dev->cmd_queues[SRQueue_Copy], 1, &submitInfo, nullptr);
	vkQueueWaitIdle(dev->cmd_queues[SRQueue_Copy]); // TODO: Use fence instead

	uintptr_t upload_bytes = (uintptr_t)(dev->arena_upload->allocated - dev->arena_upload->data);
	u64 upload_count = upload_bytes / sizeof(SRResource);
	SRResource* uploads = (SRResource*)dev->arena_upload->data;

	for (u64 i = 0; i < upload_count; ++i) {
		SRResource* resource = &uploads[i];

		if (resource->type == SRResourceType::Buffer) {
			auto* internal_buffer = (SRBuffer_Vulkan*)resource->internalState;
			dev->destruction_handler->enqueue(internal_buffer->buffer);
			dev->destruction_handler->enqueue(internal_buffer->allocation);
		}
		else {
			assert(false);
		}
		// TODO: Implement others
	}

	SRArena_Clear(dev->arena_upload);
	dev->is_upload_cmd_buffer_recording = false;
}

void SRGFXVulkan_SetupImGuiInitInfo(SRGFXDevice* device, SRFormat swapchainFormat) {
	auto* dev = (SRGFXDeviceVulkan*)device->internalState;

	ImGui::GetPlatformIO().Platform_CreateVkSurface = [](
		ImGuiViewport* viewport,
		ImU64 vkInstance,
		const void* vkAllocators,
		ImU64* outVkSurface
	) -> int {
		VkInstance instance = (VkInstance)vkInstance;
		HWND hwnd = (HWND)viewport->PlatformHandle;

		if (!hwnd) {
			hwnd = (HWND)viewport->PlatformHandleRaw;
		}

		VkWin32SurfaceCreateInfoKHR ci{};
		ci.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		ci.hinstance = GetModuleHandle(nullptr);
		ci.hwnd = hwnd;

		VkSurfaceKHR surface = VK_NULL_HANDLE;
		VkResult err = vkCreateWin32SurfaceKHR(
			instance, &ci,
			(const VkAllocationCallbacks*)vkAllocators,
			&surface
		);

		if (outVkSurface) {
			*outVkSurface = (ImU64)surface;
		}

		return err == VK_SUCCESS;
	};

	VkFormat vkSwapchainFormat = to_vk_format(swapchainFormat);
	ImGui_ImplVulkan_InitInfo initInfo = {
		.ApiVersion = VK_API_VERSION_1_4,
		.Instance = dev->instance,
		.PhysicalDevice = dev->physical_device,
		.Device = dev->device,
		.QueueFamily = dev->cmd_queue_indices[SRQueue_Universal],
		.Queue = dev->cmd_queues[SRQueue_Universal],
		.DescriptorPoolSize = IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE,
		.MinImageCount = 3, // TODO: Depends on the swapchain buffers we choose, make the function require a swapchain object to check
		.ImageCount = 3,
		.PipelineInfoMain = {
			.PipelineRenderingCreateInfo = {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
				.viewMask = 0,
				.colorAttachmentCount = 1,
				.pColorAttachmentFormats = &vkSwapchainFormat,
				.depthAttachmentFormat = VK_FORMAT_UNDEFINED,
				.stencilAttachmentFormat = VK_FORMAT_UNDEFINED
			}
		},
		.UseDynamicRendering = true,
		.CheckVkResultFn = [](VkResult err) { SR_VK_CHECK(err, ""); }
	};

	ImGui_ImplVulkan_Init(&initInfo);
}
