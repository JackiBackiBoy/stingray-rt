#define VOLK_IMPLEMENTATION
#define VMA_IMPLEMENTATION

#include "GraphicsDevice_Vulkan.h"
#include "Graphics/GraphicsTypes.h"
#include "Graphics/Vulkan/GraphicsHelpers_Vulkan.h"
#include "Graphics/Vulkan/GraphicsTypes_Vulkan.h"
#include "Core/Logger.h"
#include "Core/System/MonitorEnumerator.h"

#include <imgui.h>
#include <imgui_impl_vulkan.h>

#include <algorithm>
#include <cassert>
#include <memory>
#include <vector>
#include <stdexcept>
#include <Windows.h>

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

// ------------------------------ Impl Definition ------------------------------
struct SRGraphicsDevice_Vulkan::Impl {
	Impl(SRWindow& window) : m_Window(window) {}
	~Impl();

	SRWindow& m_Window;
	VkInstance m_Instance = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT m_DebugMessenger = VK_NULL_HANDLE;
	VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
	VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
	VkDevice m_Device = VK_NULL_HANDLE;
	VmaAllocator m_Allocator = VMA_NULL;
	VkCommandPool m_CommandPools[SRQueue_COUNT][FRAMES_IN_FLIGHT] = {};
	VkQueue m_CommandQueues[SRQueue_COUNT] = {};
	u32 m_QueueIndices[SRQueue_COUNT] = {};
	VkSemaphore m_FrameFences[SRQueue_COUNT] = {};
	VkSemaphore m_ImageAvailableSemaphores[FRAMES_IN_FLIGHT] = {};
	VkSemaphore m_RenderFinishedSemaphores[FRAMES_IN_FLIGHT] = {};
	VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
	VkDescriptorSet m_ResourceDescriptorSet = VK_NULL_HANDLE; // CBV/SRV/UAV descriptor set
	VkDescriptorSetLayout m_ResourceDescriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorSetLayout m_PushDescriptorSetLayout = VK_NULL_HANDLE;
	SRDescriptorHeap_Vulkan m_CbvSrvUavDescriptorHeap = { VK_DESCRIPTOR_TYPE_MUTABLE_EXT, 32000 };
	SRDescriptorHeap_Vulkan m_SamplerDescriptorHeap = { VK_DESCRIPTOR_TYPE_SAMPLER, MAX_SAMPLER_DESCRIPTORS };
	SRPipeline_Vulkan* m_ActivePipeline = nullptr;
	std::unique_ptr<SRDestructionHandler_Vulkan> m_DestructionHandler;

	u64 m_NextGPUSignalValue = 1;
	u64 m_FrameDoneValue[SRQueue_COUNT][FRAMES_IN_FLIGHT] = {};
	std::vector<std::unique_ptr<SRCmdList_Vulkan>> m_PerFrameCmdLists[FRAMES_IN_FLIGHT];
	size_t m_PerFrameCmdListCounters[FRAMES_IN_FLIGHT] = {};
	u32 m_FrameIndex = 0;
	u32 m_ImageIndex = 0;
	u64 m_FrameCounter = 0;

	bool m_DebugUtilsAvailable = false;

	void create_instance();
	void create_debug_messenger();
	void create_surface();
	void create_device();
	void create_vulkan_memory_allocator();
	void create_command_pools();
	void create_sync_objects();
	void create_descriptors();
	void create_destruction_handler();

	void create_swapchain(const SRSwapchainInfo& info, SRSwapchain& swapchain);
	void create_pipeline(const SRPipelineInfo& info, SRPipeline& pipeline);
	void create_buffer(const SRBufferInfo& info, SRBuffer& buffer, const void* data);
	void create_texture(const SRTextureInfo& info, SRTexture& texture, const SRSubresourceData* data);
	void create_sampler(const SRSamplerInfo& info, SRSampler& sampler);

	void bind_pipeline(const SRPipeline& pipeline, const SRCmdList& cmdList);
	void bind_vertex_buffer(const SRBuffer& buffer, const SRCmdList& cmdList);
	void bind_index_buffer(const SRBuffer& buffer, const SRCmdList& cmdList);
	void bind_root_constant_buffer(const SRBuffer& buffer, const SRCmdList& cmdList);
	void push_constants(const void* data, u32 size, const SRCmdList& cmdList);
	void barrier(const SRBarrier* pBarriers, u32 numBarriers, const SRCmdList& cmdList);

	SRCmdList begin_command_list(SRQueue queue);
	void begin_render_pass(const SRSwapchain& swapchain, const SRCmdList& cmdList);
	void begin_render_pass(const SRPassInfo& passInfo, const SRCmdList& cmdList);
	void end_render_pass(const SRSwapchain& swapchain, const SRCmdList& cmdList);
	void end_render_pass(const SRCmdList& cmdList);
	void submit_command_lists(const SRSwapchain& swapchain);

	void draw(u32 vtxCount, u32 startVtx, const SRCmdList& cmdList);
	void draw_indexed(u32 idxCount, u32 startIdx, u32 baseVtx, const SRCmdList& cmdList);

	SRDescriptorIndex get_descriptor_index_srv(const SRResource& resource);
	SRShaderPlatformInfo get_shader_platform_info();
	void wait_for_gpu();
	void setup_imgui_init_info(SRFormat swapchainFormat);

	// NOTE: TEMPORARY STUFF
	void flush_initial_uploads();
	VkCommandPool m_UploadCmdPool = VK_NULL_HANDLE;
	VkCommandBuffer m_UploadCmdBuffer = VK_NULL_HANDLE;
	bool m_IsUploadCmdBufferRecording = false;
	// END OF TEMPORARY STUFF

	static constexpr SRShaderPlatformInfo m_ShaderPlatformInfo = {
		SRShaderCompileTarget::SPIRV,
		"glsl_460"
	};
	static constexpr u32 MAX_UNIFORM_BUFFER_DESCRIPTORS = 64;
	static constexpr u32 MAX_SAMPLER_DESCRIPTORS = 32;
	static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData
	);
	static void populate_debug_messenger_create_info(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
};

// -------------------------- Impl Method Definitions --------------------------
SRGraphicsDevice_Vulkan::Impl::~Impl() {
#ifdef _DEBUG
	if (m_DebugUtilsAvailable) {
		m_DestructionHandler->enqueue(m_DebugMessenger);
	}
#endif

	m_DestructionHandler->enqueue(m_Surface);

	// Command pools
	for (u32 q = 0; q < SRQueue_COUNT; ++q) {
		for (u32 f = 0; f < FRAMES_IN_FLIGHT; ++f) {
			m_DestructionHandler->enqueue(m_CommandPools[q][f]);
		}
	}
	m_DestructionHandler->enqueue(m_UploadCmdPool);

	// Fences (timeline semaphores)
	for (u32 q = 0; q < SRQueue_COUNT; ++q) {
		m_DestructionHandler->enqueue(m_FrameFences[q]);
	}

	// Semaphores
	for (u32 f = 0; f < FRAMES_IN_FLIGHT; f++) {
		m_DestructionHandler->enqueue(m_ImageAvailableSemaphores[f]);
		m_DestructionHandler->enqueue(m_RenderFinishedSemaphores[f]);
	}

	// Descriptor pool
	m_DestructionHandler->enqueue(m_DescriptorPool);

	// Descriptor set layouts
	m_DestructionHandler->enqueue(m_PushDescriptorSetLayout);
	m_DestructionHandler->enqueue(m_ResourceDescriptorSetLayout);
}

void SRGraphicsDevice_Vulkan::Impl::create_instance() {
	SR_VK_CHECK(volkInitialize(), "Volk initialization");
	SRLOG_INFO_CAT(SRLOG_CAT_VULKAN, "Volk successfully initialized");

	const VkApplicationInfo appInfo = {
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
				m_DebugUtilsAvailable = true;
				break;
			}
		}

		// Create instance-level debug messenger, only used during creation
		VkDebugUtilsMessengerCreateInfoEXT debugMessengerInfo;

		if (m_DebugUtilsAvailable) {
			populate_debug_messenger_create_info(debugMessengerInfo);
			instanceInfo.pNext = &debugMessengerInfo;
		}
		else {
			SRLOG_WARN_CAT(SRLOG_CAT_VULKAN, "VK_EXT_debug_utils not available. Debug messenger will not be created");
		}
	#endif

	instanceInfo.enabledExtensionCount = static_cast<u32>(enabledExts.size());
	instanceInfo.ppEnabledExtensionNames = enabledExts.data();

	// TODO: Investigate custom Vulkan allocator
	SR_VK_CHECK(vkCreateInstance(&instanceInfo, nullptr, &m_Instance), "Instance creation");

	volkLoadInstanceOnly(m_Instance);
}

void SRGraphicsDevice_Vulkan::Impl::create_debug_messenger() {
	#ifdef _DEBUG
		if (!m_DebugUtilsAvailable) {
			return;
		}

		auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
			vkGetInstanceProcAddr(m_Instance, "vkCreateDebugUtilsMessengerEXT")
		);
		if (func == nullptr) {
			SRLOG_ERROR_CAT(SRLOG_CAT_VULKAN, "vkGetInstanceProcAddr could not be obtained");
			return;
		}

		VkDebugUtilsMessengerCreateInfoEXT createInfo;
		populate_debug_messenger_create_info(createInfo);
		SR_VK_CHECK(func(m_Instance, &createInfo, nullptr, &m_DebugMessenger), "Debug messenger creation");
	#else
		return;
	#endif
}

void SRGraphicsDevice_Vulkan::Impl::create_surface() {
	VkWin32SurfaceCreateInfoKHR win32SurfaceInfo = {
		.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
		.hinstance = (HINSTANCE)GetModuleHandle(nullptr),
		.hwnd = (HWND)m_Window.get_internal_handle()
	};

	SR_VK_CHECK(vkCreateWin32SurfaceKHR(m_Instance, &win32SurfaceInfo, nullptr, &m_Surface), "Win32 surface creation");
}

void SRGraphicsDevice_Vulkan::Impl::create_device() {
	u32 numDevices = 0;
	SR_VK_CHECK(vkEnumeratePhysicalDevices(m_Instance, &numDevices, nullptr), "Physical device enumeration");

	if (numDevices == 0) {
		SRLOG_CRITICAL_CAT(SRLOG_CAT_VULKAN, "No GPU with Vulkan support was found");
		throw std::runtime_error("Vulkan error: No GPU with Vulkan support was found");
	}

	std::vector<VkPhysicalDevice> devices(numDevices);
	SR_VK_CHECK(vkEnumeratePhysicalDevices(m_Instance, &numDevices, devices.data()), "Physical device enumeration");

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
		const bool isApi14Plus = (
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

		REQUIRE(vk14Features.pushDescriptor,                                      "feature: pushDescriptor");
		REQUIRE(vk13Features.synchronization2,                                    "feature: synchronization2");
		REQUIRE(vk13Features.dynamicRendering,                                    "feature: dynamicRendering");
		REQUIRE(vk12Features.timelineSemaphore,                                   "feature: timelineSemaphore");
		REQUIRE(vk12Features.bufferDeviceAddress,                                 "feature: bufferDeviceAddress");
		REQUIRE(rtPipelineFeatures.rayTracingPipeline,                            "feature: rayTracingPipeline");
		REQUIRE(asFeatures.accelerationStructure,                                 "feature: accelerationStructure");
		REQUIRE(meshShaderFeatures.meshShader,                                    "feature: meshShader");
		REQUIRE(vk12Features.descriptorIndexing,                                  "feature: descriptorIndexing");
		REQUIRE(vk12Features.descriptorBindingPartiallyBound,                     "feature: descriptorBindingPartiallyBound");
		REQUIRE(vk12Features.runtimeDescriptorArray,                              "feature: runtimeDescriptorArray");
		REQUIRE(vk12Features.descriptorBindingSampledImageUpdateAfterBind,        "feature: descriptorBindingSampledImageUpdateAfterBind");
		REQUIRE(vk12Features.descriptorBindingStorageBufferUpdateAfterBind,       "feature: descriptorBindingStorageBufferUpdateAfterBind");
		REQUIRE(vk12Features.descriptorBindingStorageImageUpdateAfterBind,        "feature: descriptorBindingStorageImageUpdateAfterBind");
		REQUIRE(vk12Features.descriptorBindingUniformBufferUpdateAfterBind,       "feature: descriptorBindingUniformBufferUpdateAfterBind");
		REQUIRE(vk12Features.shaderSampledImageArrayNonUniformIndexing,           "feature: shaderSampledImageArrayNonUniformIndexing");
		REQUIRE(vk12Features.shaderStorageBufferArrayNonUniformIndexing,          "feature: shaderStorageBufferArrayNonUniformIndexing");
		REQUIRE(vk12Features.shaderStorageImageArrayNonUniformIndexing,           "feature: shaderStorageImageArrayNonUniformIndexing");
		REQUIRE(vk12Features.shaderUniformBufferArrayNonUniformIndexing,          "feature: shaderUniformBufferArrayNonUniformIndexing");
		REQUIRE(asFeatures.descriptorBindingAccelerationStructureUpdateAfterBind, "feature: descriptorBindingAccelerationStructureUpdateAfterBind");
		REQUIRE(deviceFeatures.features.samplerAnisotropy,                        "feature: samplerAnisotropy");
		REQUIRE(mutableDescriptorTypeFeatures.mutableDescriptorType,              "feature: mutableDescriptorType");

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
			const bool hasGraphicsBit = (family.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
			const bool hasComputeBit = (family.queueFlags & VK_QUEUE_COMPUTE_BIT) != 0;
			const bool hasCopyBit = (family.queueFlags & VK_QUEUE_TRANSFER_BIT) != 0;

			if (hasGraphicsBit && hasComputeBit) { // Universal queue
				// Check present support
				VkBool32 hasPresentSupport = VK_FALSE;
				SR_VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_Surface, &hasPresentSupport), "Query presentation support");

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

		m_QueueIndices[SRQueue_Universal] = universalQueueFamilyIdx;
		m_QueueIndices[SRQueue_Compute] = dedicatedComputeQueueFamilyIdx;
		m_QueueIndices[SRQueue_Copy] = dedicatedCopyQueueFamilyIdx;

		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		const float queuePriority = 1.0f; // TODO: Might not always be the best?

		const VkDeviceQueueCreateInfo univeralQueueInfo = {
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.queueFamilyIndex = universalQueueFamilyIdx,
			.queueCount = 1,
			.pQueuePriorities = &queuePriority
		};
		queueCreateInfos.push_back(univeralQueueInfo);

		const VkDeviceQueueCreateInfo dedicatedComputeQueueInfo = {
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.queueFamilyIndex = dedicatedComputeQueueFamilyIdx,
			.queueCount = 1,
			.pQueuePriorities = &queuePriority
		};
		queueCreateInfos.push_back(dedicatedComputeQueueInfo);

		if (dedicatedCopyQueueFamilyIdx != ~0) {
			const VkDeviceQueueCreateInfo dedicatedCopyQueueInfo = {
				.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
				.queueFamilyIndex = dedicatedCopyQueueFamilyIdx,
				.queueCount = 1,
				.pQueuePriorities = &queuePriority
			};
			queueCreateInfos.push_back(dedicatedCopyQueueInfo);
		}

		// TODO: Perhaps have requirements for available present modes? FIFO is always guaranteed at least
		const VkDeviceCreateInfo deviceInfo = {
			.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			.pNext = &enableDeviceFeat,
			.queueCreateInfoCount = static_cast<u32>(queueCreateInfos.size()),
			.pQueueCreateInfos = queueCreateInfos.data(),
			.enabledExtensionCount = static_cast<u32>(enabledExts.size()),
			.ppEnabledExtensionNames = enabledExts.data(),
		};

		SR_VK_CHECK(vkCreateDevice(device, &deviceInfo, nullptr, &m_Device), "Device creation");
		pickedDeviceIdx = i;
		break;
	}

	if (pickedDeviceIdx == ~0) {
		SRLOG_CRITICAL_CAT(SRLOG_CAT_VULKAN, "No suitable GPU found");
		throw std::runtime_error("VULKAN ERROR: No suitable GPU found");
	}

	SRLOG_DEBUG_CAT(SRLOG_CAT_VULKAN, "Picked [GPU%u] %s", pickedDeviceIdx, deviceName);

	m_PhysicalDevice = devices[pickedDeviceIdx];

	volkLoadDevice(m_Device);
	vkGetDeviceQueue(m_Device, m_QueueIndices[SRQueue_Universal], 0, &m_CommandQueues[SRQueue_Universal]);
	vkGetDeviceQueue(m_Device, m_QueueIndices[SRQueue_Compute], 0, &m_CommandQueues[SRQueue_Compute]);
	vkGetDeviceQueue(m_Device, m_QueueIndices[SRQueue_Copy], 0, &m_CommandQueues[SRQueue_Copy]);
}

void SRGraphicsDevice_Vulkan::Impl::create_vulkan_memory_allocator() {
	const VmaVulkanFunctions volkFunctions = {
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

	const VmaAllocatorCreateInfo allocatorInfo = {
		.physicalDevice = m_PhysicalDevice,
		.device = m_Device,
		.preferredLargeHeapBlockSize = 0, // 256 MB default
		.pAllocationCallbacks = nullptr,
		.pDeviceMemoryCallbacks = nullptr,
		.pHeapSizeLimit = nullptr,
		.pVulkanFunctions = &volkFunctions,
		.instance = m_Instance,
		.vulkanApiVersion = VK_API_VERSION_1_4,
		.pTypeExternalMemoryHandleTypes = nullptr
	};

	SR_VK_CHECK(vmaCreateAllocator(&allocatorInfo, &m_Allocator), "Create Vulkan Memory Allocator");
}

void SRGraphicsDevice_Vulkan::Impl::create_command_pools() {
	VkCommandPoolCreateInfo poolInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, // TODO: Look into whether VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT might OCCASIONALLY be useful
	};

	for (u32 q = 0; q < SRQueue_COUNT; ++q) {
		poolInfo.queueFamilyIndex = m_QueueIndices[q];

		for (u32 f = 0; f < FRAMES_IN_FLIGHT; ++f) {
			SR_VK_CHECK(vkCreateCommandPool(m_Device, &poolInfo, nullptr, &m_CommandPools[q][f]), "Command pool creation");
		}
	}

	// TEMPORARY
	poolInfo.queueFamilyIndex = m_QueueIndices[SRQueue_Copy];
	SR_VK_CHECK(vkCreateCommandPool(m_Device, &poolInfo, nullptr, &m_UploadCmdPool), "Create upload command pool");

	// Create initial upload command buffer
	const VkCommandBufferAllocateInfo allocInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = m_UploadCmdPool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1
	};

	SR_VK_CHECK(vkAllocateCommandBuffers(m_Device, &allocInfo, &m_UploadCmdBuffer), "Upload command buffer creation");
}

void SRGraphicsDevice_Vulkan::Impl::create_sync_objects() {
	// Frame timeline semaphore
	const VkSemaphoreTypeCreateInfo timelineInfo = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
		.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
		.initialValue = 0
	};

	VkSemaphoreCreateInfo semaphoreInfo = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		.pNext = &timelineInfo
	};

	for (u32 q = 0; q < SRQueue_COUNT; ++q) {
		SR_VK_CHECK(vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &(m_FrameFences[q])), "Timeline semaphore creation");
	}

	// Image available and render finished semaphores
	semaphoreInfo.pNext = nullptr;
	for (u32 f = 0; f < FRAMES_IN_FLIGHT; ++f) {
		SR_VK_CHECK(vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &m_ImageAvailableSemaphores[f]), "Image-available semaphore creation");
		SR_VK_CHECK(vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &m_RenderFinishedSemaphores[f]), "Render-finished semaphore creation");
	}
}

void SRGraphicsDevice_Vulkan::Impl::create_descriptors() {
	// Bindless descriptors (set 0)
	std::vector<SRDescriptorHeap_Vulkan*> descriptorHeaps = {
		&m_CbvSrvUavDescriptorHeap,
		&m_SamplerDescriptorHeap,
	};
	std::vector<VkDescriptorPoolSize> poolSizes;
	std::vector<VkDescriptorBindingFlags> bindingFlags;
	std::vector<VkDescriptorSetLayoutBinding> layoutBindings;
	poolSizes.reserve(descriptorHeaps.size());
	bindingFlags.reserve(descriptorHeaps.size());
	layoutBindings.reserve(descriptorHeaps.size());

	for (size_t i = 0; i < descriptorHeaps.size(); ++i) {
		SRDescriptorHeap_Vulkan* heap = descriptorHeaps[i];
		VkDescriptorType descriptorType = heap->get_type();
		u32 descriptorCount = heap->get_capacity();

		VkDescriptorPoolSize poolSize = { descriptorType, descriptorCount };
		VkDescriptorBindingFlags flags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
		VkDescriptorSetLayoutBinding layoutBinding = {
			.binding = static_cast<u32>(i),
			.descriptorType = heap->get_type(),
			.descriptorCount = heap->get_capacity(),
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
	SR_VK_CHECK(vkCreateDescriptorPool(m_Device, &poolInfo, nullptr, &m_DescriptorPool), "Create descriptor pool");

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
	SR_VK_CHECK(vkCreateDescriptorSetLayout(m_Device, &setLayoutInfo, nullptr, &m_ResourceDescriptorSetLayout), "Create descriptor set layout");

	// Descriptor set
	VkDescriptorSetAllocateInfo descriptorSetAllocInfo = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = m_DescriptorPool,
		.descriptorSetCount = 1,
		.pSetLayouts = &m_ResourceDescriptorSetLayout
	};
	SR_VK_CHECK(vkAllocateDescriptorSets(m_Device, &descriptorSetAllocInfo, &m_ResourceDescriptorSet), "Allocate descriptor sets");

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
	SR_VK_CHECK(vkCreateDescriptorSetLayout(m_Device, &pushLayoutInfo, nullptr, &m_PushDescriptorSetLayout), "Create push-descriptor set layout");
}

void SRGraphicsDevice_Vulkan::Impl::create_destruction_handler() {
	m_DestructionHandler = std::make_unique<SRDestructionHandler_Vulkan>(m_Device, m_Instance, m_Allocator);
}

void SRGraphicsDevice_Vulkan::Impl::create_swapchain(const SRSwapchainInfo& info, SRSwapchain& swapchain) {
	auto internalSwapchain = std::make_shared<SRSwapchain_Vulkan>();
	internalSwapchain->destructionHandler = m_DestructionHandler.get();

	swapchain.info = info;
	swapchain.internalState = internalSwapchain;

	const SRSwapchainSupportInfo supportInfo = SRVulkanHelpers::query_swapchain_support(m_PhysicalDevice, m_Surface);
	const VkSurfaceFormatKHR surfaceFormat = SRVulkanHelpers::pick_surface_format(
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
		int width;
		int height;
		m_Window.get_client_size(&width, &height);
		extent = { static_cast<u32>(width), static_cast<u32>(height) };

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

	// TODO: Swapchain recreation (check if internal state is nullptr)
	const VkSwapchainCreateInfoKHR createInfo = {
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.flags = 0, // TODO: Investigate swapchain flags
		.surface = m_Surface,
		.minImageCount = static_cast<u32>(info.numBuffers),
		.imageFormat = surfaceFormat.format,
		.imageColorSpace = surfaceFormat.colorSpace,
		.imageExtent = extent,
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.preTransform = supportInfo.capabilities.currentTransform,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode = SRVulkanHelpers::pick_present_mode(info.vSync, supportInfo.presentModes),
		.clipped = VK_TRUE
	};
	SR_VK_CHECK(vkCreateSwapchainKHR(m_Device, &createInfo, nullptr, &internalSwapchain->swapchain), "Swapchain creation");

	// Swapchain images
	u32 numImages;
	SR_VK_CHECK(vkGetSwapchainImagesKHR(m_Device, internalSwapchain->swapchain, &numImages, nullptr), "Get swapchain images");
	internalSwapchain->images.resize(numImages);
	internalSwapchain->imageViews.resize(numImages);
	SR_VK_CHECK(vkGetSwapchainImagesKHR(m_Device, internalSwapchain->swapchain, &numImages, internalSwapchain->images.data()), "Get swapchain images");

	// Swapchain image views
	for (u32 i = 0; i < numImages; ++i) {
		const VkImageViewCreateInfo imageViewInfo = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = internalSwapchain->images[i],
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

		SR_VK_CHECK(vkCreateImageView(m_Device, &imageViewInfo, nullptr, &internalSwapchain->imageViews[i]), "Swapchain image view creation");
	}
}

void SRGraphicsDevice_Vulkan::Impl::create_pipeline(const SRPipelineInfo& info, SRPipeline& pipeline) {
	auto internalPipeline = std::make_shared<SRPipeline_Vulkan>();
	internalPipeline->destructionHandler = m_DestructionHandler.get();

	pipeline.info = info;
	pipeline.internalState = internalPipeline;

	std::vector<VkShaderModule> shaderModules;
	std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

	// TODO: Would be nice to have pipeline caching
	if (info.vertexShader != nullptr) {
		VkShaderModule shaderModule = SRVulkanHelpers::create_shader_module(m_Device, info.vertexShader);
		assert(shaderModule);
		shaderModules.push_back(shaderModule);
		
		const VkPipelineShaderStageCreateInfo shaderStageInfo = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = shaderModule,
			.pName = info.vertexShader->entryPoint,
			.pSpecializationInfo = nullptr
		};
		shaderStages.push_back(shaderStageInfo);
	}
	if (info.pixelShader != nullptr) {
		VkShaderModule shaderModule = SRVulkanHelpers::create_shader_module(m_Device, info.pixelShader);
		assert(shaderModule);
		shaderModules.push_back(shaderModule);

		const VkPipelineShaderStageCreateInfo shaderStageInfo = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = shaderModule,
			.pName = info.pixelShader->entryPoint,
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
	std::vector<VkVertexInputAttributeDescription> attributeDescriptions(info.inputLayout.elements.size());
	u32 offset = 0;

	for (size_t i = 0; i < attributeDescriptions.size(); i++) {
		attributeDescriptions[i].binding = 0;
		attributeDescriptions[i].location = static_cast<u32>(i); // TODO: Doesn't work for all formats
		attributeDescriptions[i].format = to_vk_format(info.inputLayout.elements[i].format);
		attributeDescriptions[i].offset = offset;

		offset += SRGraphicsHelpers::get_format_stride(info.inputLayout.elements[i].format);
	}

	// TODO: For now we only allow one binding description
	const VkVertexInputBindingDescription bindingDescription = {
		.binding = 0,
		.stride = offset, // total offset is equivalent to stride
		.inputRate = VK_VERTEX_INPUT_RATE_VERTEX
	};

	const VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = attributeDescriptions.empty() ? 0U : 1U,
		.pVertexBindingDescriptions = attributeDescriptions.empty() ? nullptr : &bindingDescription,
		.vertexAttributeDescriptionCount = static_cast<u32>(attributeDescriptions.size()),
		.pVertexAttributeDescriptions = attributeDescriptions.empty() ? nullptr : attributeDescriptions.data()
	};

	const VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, // TODO: Support other topologies
		.primitiveRestartEnable = VK_FALSE
	};

	const VkPipelineViewportStateCreateInfo viewportState = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.scissorCount = 1
	};

	const VkPipelineRasterizationStateCreateInfo rasterizerInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.depthClampEnable = VK_FALSE,
		.rasterizerDiscardEnable = VK_FALSE,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.cullMode = to_vk_cull_mode(info.rasterizerState.cullMode),
		.frontFace = info.rasterizerState.frontCW ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE,
		.depthBiasEnable = VK_FALSE,
		.depthBiasConstantFactor = 0.0f,
		.depthBiasClamp = 0.0f,
		.depthBiasSlopeFactor = 0.0f,
		.lineWidth = 1.0f
	};

	const VkPipelineMultisampleStateCreateInfo multisamplingInfo = {
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
	for (u32 i = 0; i < info.numRenderTargets; ++i) {
		const SRBlendState::RenderTargetBlendState& blendState = info.blendState.renderTargetBlendStates[i];

		// TODO: Make dynamic
		const VkPipelineColorBlendAttachmentState colorBlendState = {
			.blendEnable = blendState.blendEnable ? VK_TRUE : VK_FALSE,
			.srcColorBlendFactor = to_vk_blend(blendState.srcBlend),
			.dstColorBlendFactor = to_vk_blend(blendState.dstBlend),
			.colorBlendOp = to_vk_blend_op(blendState.blendOp),
			.srcAlphaBlendFactor = to_vk_blend(blendState.srcBlendAlpha),
			.dstAlphaBlendFactor = to_vk_blend(blendState.dstBlendAlpha),
			.alphaBlendOp = to_vk_blend_op(blendState.blendOpAlpha),
			.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
		};

		colorBlendStates.push_back(colorBlendState);
	}

	const VkPipelineColorBlendStateCreateInfo colorBlendInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.logicOpEnable = VK_FALSE,
		.logicOp = VK_LOGIC_OP_COPY,
		.attachmentCount = static_cast<u32>(colorBlendStates.size()),
		.pAttachments = colorBlendStates.data(),
		.blendConstants = { 0.0f, 0.0f, 0.0f, 0.0f }
	};

	// Descriptors
	const VkPushConstantRange pushConstantRange = {
		.stageFlags = VK_SHADER_STAGE_ALL,
		.offset = 0,
		.size = 128
	};

	const VkDescriptorSetLayout setLayouts[] = {
		m_ResourceDescriptorSetLayout, // set 0
		m_PushDescriptorSetLayout // set 1
	};

	const VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = std::size(setLayouts),
		.pSetLayouts = setLayouts,
		.pushConstantRangeCount = 1,
		.pPushConstantRanges = &pushConstantRange
	};

	SR_VK_CHECK(vkCreatePipelineLayout(
		m_Device,
		&pipelineLayoutInfo,
		nullptr,
		&internalPipeline->pipelineLayout
	), "Create pipeline layout");

	std::vector<VkFormat> colorAttachmentFormats = {};
	colorAttachmentFormats.reserve(static_cast<size_t>(info.numRenderTargets));

	for (size_t i = 0; i < info.numRenderTargets; ++i) {
		colorAttachmentFormats.push_back(to_vk_format(info.renderTargetFormats[i]));
	}

	// TODO: Stencil format unspecified right now
	const VkPipelineRenderingCreateInfo pipelineRenderingInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		.colorAttachmentCount = info.numRenderTargets,
		.pColorAttachmentFormats = colorAttachmentFormats.data(),
		.depthAttachmentFormat = to_vk_format(info.depthStencilFormat)
	};

	const VkPipelineDepthStencilStateCreateInfo depthStencilInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.depthTestEnable = info.depthStencilState.depthEnable ? VK_TRUE : VK_FALSE,
		.depthWriteEnable = info.depthStencilState.depthWriteMask == SRDepthWriteMask::Zero ? VK_FALSE : VK_TRUE,
		.depthCompareOp = to_vk_comparison_func(info.depthStencilState.depthFunction),
		.depthBoundsTestEnable = VK_FALSE,
		.stencilTestEnable = VK_FALSE,
		.minDepthBounds = 0.0f,
		.maxDepthBounds = 1.0f
	};

	const VkGraphicsPipelineCreateInfo pipelineInfo = {
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
		m_Device,
		nullptr,
		1,
		&pipelineInfo,
		nullptr,
		&internalPipeline->pipeline
	), "Create graphics pipeline");

	for (const auto& shaderModule : shaderModules) {
		m_DestructionHandler->enqueue(shaderModule);
	}
}

// TODO: Add support for ReBar devices
void SRGraphicsDevice_Vulkan::Impl::create_buffer(const SRBufferInfo& info, SRBuffer& buffer, const void* data) {
	auto internalBuffer = std::make_shared<SRBuffer_Vulkan>();
	internalBuffer->destructionHandler = m_DestructionHandler.get();

	buffer.info = info;
	buffer.internalState = internalBuffer;
	buffer.mappedData = nullptr;
	buffer.mappedSize = 0;

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
		m_Allocator,
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
		create_buffer(stagingBufferInfo, stagingBuffer, data);
		auto internalStagingBuffer = to_vk_internal(stagingBuffer);

		// Copy staging buffer into target buffer
		if (!m_IsUploadCmdBufferRecording) {
			const VkCommandBufferBeginInfo beginInfo = {
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
				.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
			};

			SR_VK_CHECK(vkResetCommandPool(m_Device, m_UploadCmdPool, 0), "Reset command pool");
			SR_VK_CHECK(vkBeginCommandBuffer(m_UploadCmdBuffer, &beginInfo), "Begin command buffer");
			m_IsUploadCmdBufferRecording = true;
		}

		const VkBufferCopy copyRegion = {
			.srcOffset = 0,
			.dstOffset = 0,
			.size = info.size
		};
		vkCmdCopyBuffer(
			m_UploadCmdBuffer,
			internalStagingBuffer->buffer,
			internalBuffer->buffer,
			1,
			&copyRegion
		);
	}
	else if (info.usage == SRUsage::Upload) {
		buffer.mappedData = internalBuffer->allocation->GetMappedData();
		buffer.mappedSize = info.size;

		if (data != nullptr) {
			std::memcpy(buffer.mappedData, data, info.size);
		}
	}

	// TODO: Descriptors (non UBO that is)
}

void SRGraphicsDevice_Vulkan::Impl::create_texture(const SRTextureInfo& info, SRTexture& texture, const SRSubresourceData* data) {
	assert(info.usage == SRUsage::Default);

	auto internalTexture = std::make_shared<SRTexture_Vulkan>();
	internalTexture->destructionHandler = m_DestructionHandler.get();

	texture.type = SRResourceType::Texture;
	texture.info = info;
	texture.internalState = internalTexture;

	VkImageCreateInfo imageInfo = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.flags = 0,
		.imageType = VK_IMAGE_TYPE_2D, // TODO: Make dynamic
		.format = to_vk_format(info.format),
		.extent = { info.width, info.height, info.depth },
		.mipLevels = info.mipLevels,
		.arrayLayers = info.arraySize,
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

	if (has_flag(info.bindFlags, SRBindFlag::ShaderResource)) {
		imageInfo.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
		accessFlags |= VK_ACCESS_2_SHADER_READ_BIT;
	}
	if (has_flag(info.bindFlags, SRBindFlag::UnorderedAccess)) {
		imageInfo.usage |= VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		//accessFlags = VK_ACCESS_2_SHA
	}

	if (has_flag(info.bindFlags, SRBindFlag::RenderTarget)) {
		imageInfo.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		accessFlags |= VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;
		accessFlags |= VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	}
	else if (has_flag(info.bindFlags, SRBindFlag::DepthStencil)) {
		imageInfo.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		accessFlags |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
		accessFlags |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	}

	VmaAllocationInfo allocInfo = {};
	SR_VK_CHECK(vmaCreateImage(
		m_Allocator,
		&imageInfo,
		&allocCreateInfo,
		&internalTexture->image,
		&internalTexture->allocation,
		&allocInfo
	), "Create image");

	const bool isDepthFormat = SRGraphicsHelpers::is_depth_format(info.format);
	const VkImageAspectFlags aspectMask = isDepthFormat ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
	const VkImageViewCreateInfo imageViewInfo = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = internalTexture->image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D, // TODO: Make dynamic
		.format = to_vk_format(info.format),
		.components = {
			.r = VK_COMPONENT_SWIZZLE_IDENTITY,
			.g = VK_COMPONENT_SWIZZLE_IDENTITY,
			.b = VK_COMPONENT_SWIZZLE_IDENTITY,
			.a = VK_COMPONENT_SWIZZLE_IDENTITY,
		},
		.subresourceRange = {
			.aspectMask = aspectMask,
			.baseMipLevel = 0,
			.levelCount = info.mipLevels,
			.baseArrayLayer = 0,
			.layerCount = info.arraySize
		}
	};
	SR_VK_CHECK(vkCreateImageView(
		m_Device,
		&imageViewInfo,
		nullptr,
		&internalTexture->imageView
	), "Create image view");

	if (data && data->data) {
		// Staging buffer
		SRBufferInfo stagingBufferInfo = {
			.size = static_cast<u64>(data->rowPitch * info.height),
			.usage = SRUsage::Upload
		};

		SRBuffer stagingBuffer;
		create_buffer(stagingBufferInfo, stagingBuffer, data);
		auto internalStagingBuffer = to_vk_internal(stagingBuffer);

		// Copy staging buffer into target buffer
		if (!m_IsUploadCmdBufferRecording) {
			const VkCommandBufferBeginInfo beginInfo = {
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
				.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
			};

			SR_VK_CHECK(vkResetCommandPool(m_Device, m_UploadCmdPool, 0), "Reset command pool");
			SR_VK_CHECK(vkBeginCommandBuffer(m_UploadCmdBuffer, &beginInfo), "Begin command buffer");
			m_IsUploadCmdBufferRecording = true;
		}

		std::vector<VkBufferImageCopy> copyRegions;
		VkDeviceSize copyOffset = 0;
		u32 dataIdx = 0;

		for (u32 layer = 0; layer < info.arraySize; ++layer) {
			u32 width = info.width;
			u32 height = info.height;
			u32 depth = info.depth;

			for (u32 mip = 0; mip < info.mipLevels; ++mip) {
				SRSubresourceData subresourceData = data[dataIdx++];
				u32 texelBlockSize = 1; // TODO: For block-compressed textures, this must be 4, please fix
				u32 numTexelBlocksX = std::max(1U, width / texelBlockSize);
				u32 numTexelBlocksY = std::max(1U, height / texelBlockSize);
				u32 dstRowPitch = numTexelBlocksX * SRGraphicsHelpers::get_format_stride(info.format);
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
		const SRImageTransitionInfo transitionInfo = {
			.image = internalTexture->image,
			.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			.srcAccessMask = VK_ACCESS_2_NONE,
			.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
			.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
			.aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT
		};
		SRVulkanHelpers::transition_image_layout(transitionInfo, m_UploadCmdBuffer);

		vkCmdCopyBufferToImage(
			m_UploadCmdBuffer,
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
	if (has_flag(info.bindFlags, SRBindFlag::ShaderResource)) {
		const VkDescriptorImageInfo descriptorImageInfo = {
			.sampler = nullptr,
			.imageView = internalTexture->imageView,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};

		internalTexture->srvDescriptor = m_CbvSrvUavDescriptorHeap.get_next_index();

		const VkWriteDescriptorSet descriptorWrite = {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = m_ResourceDescriptorSet,
			.dstBinding = 0,
			.dstArrayElement = internalTexture->srvDescriptor,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
			.pImageInfo = &descriptorImageInfo
		};

		vkUpdateDescriptorSets(m_Device, 1, &descriptorWrite, 0, nullptr);
	}
}

void SRGraphicsDevice_Vulkan::Impl::create_sampler(const SRSamplerInfo& info, SRSampler& sampler) {
	auto internalSampler = std::make_shared<SRSampler_Vulkan>();
	internalSampler->destructionHandler = m_DestructionHandler.get();

	sampler.info = info;
	sampler.internalState = internalSampler;

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
		.maxLod = info.maxLOD,
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

	SR_VK_CHECK(vkCreateSampler(m_Device, &samplerCreateInfo, nullptr, &internalSampler->sampler), "Create sampler");

	// Create sampler descriptor
	// TODO: Move into GraphicsHelpers_Vulkan for cleanup purposes
	const VkDescriptorImageInfo imageInfo = {
		.sampler = internalSampler->sampler
	};

	const VkWriteDescriptorSet write = {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = m_ResourceDescriptorSet,
		.dstBinding = 1,
		.dstArrayElement = 0,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
		.pImageInfo = &imageInfo
	};

	vkUpdateDescriptorSets(
		m_Device,
		1,
		&write,
		0,
		nullptr
	);
}

void SRGraphicsDevice_Vulkan::Impl::bind_pipeline(const SRPipeline& pipeline, const SRCmdList& cmdList) {
	auto internalPipeline = to_vk_internal(pipeline);
	auto internalCmdList = to_vk_internal(cmdList);

	vkCmdBindPipeline(internalCmdList->cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, internalPipeline->pipeline);
	m_ActivePipeline = internalPipeline;

	vkCmdBindDescriptorSets(
		internalCmdList->cmdBuffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		internalPipeline->pipelineLayout,
		0,
		1,
		&m_ResourceDescriptorSet,
		0,
		nullptr
	);
}

void SRGraphicsDevice_Vulkan::Impl::bind_vertex_buffer(const SRBuffer& buffer, const SRCmdList& cmdList) {
	assert(has_flag(buffer.info.bindFlags, SRBindFlag::VertexBuffer));
	auto internalBuffer = to_vk_internal(buffer);
	auto internalCmdList = to_vk_internal(cmdList);

	const VkDeviceSize offset = 0;
	vkCmdBindVertexBuffers(internalCmdList->cmdBuffer, 0, 1, &internalBuffer->buffer, &offset);
}

void SRGraphicsDevice_Vulkan::Impl::bind_index_buffer(const SRBuffer& buffer, const SRCmdList& cmdList) {
	assert(has_flag(buffer.info.bindFlags, SRBindFlag::IndexBuffer));
	auto internalBuffer = to_vk_internal(buffer);
	auto internalCmdList = to_vk_internal(cmdList);

	vkCmdBindIndexBuffer(internalCmdList->cmdBuffer, internalBuffer->buffer, 0, VK_INDEX_TYPE_UINT32);
}

void SRGraphicsDevice_Vulkan::Impl::bind_root_constant_buffer(const SRBuffer& buffer, const SRCmdList& cmdList) {
	assert(has_flag(buffer.info.bindFlags, SRBindFlag::ConstantBuffer));
	assert(m_ActivePipeline != nullptr);

	auto internalBuffer = to_vk_internal(buffer);
	auto internalCmdList = to_vk_internal(cmdList);

	const VkDescriptorBufferInfo bufferInfo = {
		.buffer = internalBuffer->buffer,
		.offset = 0,
		.range = buffer.info.size
	};
	const VkWriteDescriptorSet writeDescriptor = {
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
		m_ActivePipeline->pipelineLayout,
		1, // set 1
		1,
		&writeDescriptor
	);
}

void SRGraphicsDevice_Vulkan::Impl::push_constants(const void* data, u32 size, const SRCmdList& cmdList) {
	assert(data != nullptr);
	assert(size <= 128);
	assert(m_ActivePipeline != nullptr);

	auto internalCmdList = to_vk_internal(cmdList);

	vkCmdPushConstants(
		internalCmdList->cmdBuffer,
		m_ActivePipeline->pipelineLayout,
		VK_SHADER_STAGE_ALL,
		0,
		size,
		data
	);
}

void SRGraphicsDevice_Vulkan::Impl::barrier(const SRBarrier* pBarriers, u32 numBarriers, const SRCmdList& cmdList) {
	if (!pBarriers || numBarriers <= 0) {
		return;
	}

	auto internalCmdList = to_vk_internal(cmdList);
	std::vector<VkImageMemoryBarrier2> vkBarriers;
	vkBarriers.reserve(numBarriers);

	// TODO: Allow for UAV and buffer barriers, not only image barriers
	for (u32 i = 0; i < numBarriers; ++i) {
		const SRBarrier& barrier = pBarriers[i];
		const bool isDepthFormat = SRGraphicsHelpers::is_depth_format(barrier.image.texture->info.format);
		auto internalTexture = to_vk_internal(*barrier.image.texture);

		const VkImageAspectFlags aspectFlag = isDepthFormat ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
		const VkImageSubresourceRange subresourceRange = {
			.aspectMask = aspectFlag,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1
		};

		const VkImageMemoryBarrier2 imageBarrier = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.pNext = nullptr,
			.srcStageMask = to_vk_pipeline_stage(barrier.image.syncBefore),
			.srcAccessMask = to_vk_access_mask(barrier.image.accessBefore),
			.dstStageMask = to_vk_pipeline_stage(barrier.image.syncAfter),
			.dstAccessMask = to_vk_access_mask(barrier.image.accessAfter),
			.oldLayout = to_vk_resource_state(barrier.image.stateBefore),
			.newLayout = to_vk_resource_state(barrier.image.stateAfter),
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = internalTexture->image,
			.subresourceRange = subresourceRange // TODO: Fix barrier subresource range to work for multiple mips if requested
		};

		vkBarriers.push_back(imageBarrier);
	}

	// TODO: Doesn't work for depth attachments nor multiple mips
	const VkDependencyInfo dependencyInfo = {
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.pNext = nullptr,
		.imageMemoryBarrierCount = static_cast<u32>(vkBarriers.size()),
		.pImageMemoryBarriers = vkBarriers.data()
	};
	vkCmdPipelineBarrier2(internalCmdList->cmdBuffer, &dependencyInfo);
}

SRCmdList SRGraphicsDevice_Vulkan::Impl::begin_command_list(SRQueue queue) {
	size_t& cmdListCounter = m_PerFrameCmdListCounters[m_FrameIndex];
	auto& cmdLists = m_PerFrameCmdLists[m_FrameIndex];

	if (cmdListCounter >= cmdLists.size()) {
		cmdLists.push_back(std::make_unique<SRCmdList_Vulkan>());
	}

	auto internalCmdList = cmdLists[cmdListCounter].get();
	if (internalCmdList->cmdBuffer == VK_NULL_HANDLE) {
		const VkCommandBufferAllocateInfo allocInfo = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool = m_CommandPools[queue][m_FrameIndex],
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = 1
		};

		SR_VK_CHECK(vkAllocateCommandBuffers(m_Device, &allocInfo, &internalCmdList->cmdBuffer), "Command buffer creation");
	}

	const VkCommandBufferBeginInfo beginInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};

	// Reset the command pool JUST BEFORE we begin command buffer recording.
	// This results in as little potential CPU waiting as possible.
	// Should only be done ONCE per frame per queue family.
	SR_VK_CHECK(vkResetCommandPool(m_Device, m_CommandPools[queue][m_FrameIndex], 0), "Reset command pool");
	SR_VK_CHECK(vkBeginCommandBuffer(internalCmdList->cmdBuffer, &beginInfo), "Begin command buffer recording");
	++cmdListCounter;

	return SRCmdList{ internalCmdList };
}

void SRGraphicsDevice_Vulkan::Impl::begin_render_pass(const SRSwapchain& swapchain, const SRCmdList& cmdList) {
	auto internalSwapchain = to_vk_internal(swapchain);
	auto internalCmdList = to_vk_internal(cmdList);

	SR_VK_CHECK(vkAcquireNextImageKHR(
		m_Device,
		internalSwapchain->swapchain,
		UINT64_MAX,
		m_ImageAvailableSemaphores[m_FrameIndex],
		nullptr,
		&m_ImageIndex
	), "Acquire next swapchain image");

	const SRImageTransitionInfo transitionInfo = {
		.image = internalSwapchain->images[m_ImageIndex],
		.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.srcAccessMask = VK_ACCESS_2_NONE,
		.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
		.srcStageMask = VK_PIPELINE_STAGE_2_NONE,
		.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		.aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT
	};
	SRVulkanHelpers::transition_image_layout(transitionInfo, internalCmdList->cmdBuffer);

	const VkClearValue clearColor = {
		.color = { 0.0f, 0.0f, 0.0f, 1.0f }
	};

	const VkRenderingAttachmentInfo colorAttachmentInfo = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
		.imageView = internalSwapchain->imageViews[m_ImageIndex],
		.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.clearValue = clearColor
	};

	// TODO: Depth attachment
	const VkRect2D area{
		.offset = { 0, 0 },
		.extent = internalSwapchain->extent
	};

	const VkRenderingInfo renderInfo = {
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

void SRGraphicsDevice_Vulkan::Impl::begin_render_pass(const SRPassInfo& passInfo, const SRCmdList& cmdList) {
	auto internalCmdList = to_vk_internal(cmdList);

	std::vector<VkRenderingAttachmentInfo> colorAttachmentInfos;
	VkRenderingAttachmentInfo depthAttachmentInfo;
	colorAttachmentInfos.reserve(passInfo.numColorAttachments);

	VkRect2D renderArea = {
		.offset = { 0, 0 },
		.extent = { 0, 0 }
	};

	for (size_t i = 0; i < passInfo.numColorAttachments; ++i) {
		const SRPassInfo::Attachment& attachment = passInfo.colorAttachments[i];
		auto internalTexture = to_vk_internal(*attachment.texture);
		assert(internalTexture);

		renderArea.extent.width = std::max(renderArea.extent.width, attachment.texture->info.width);
		renderArea.extent.height = std::max(renderArea.extent.height, attachment.texture->info.height);

		const VkRenderingAttachmentInfo attachmentInfo = {
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = internalTexture->imageView,
			.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.loadOp = to_vk_load_op(attachment.loadOp),
			.storeOp = to_vk_store_op(attachment.storeOp),
			.clearValue = { .color = { 0.0f, 0.0f, 0.0f, 1.0f } }
		};
		colorAttachmentInfos.push_back(attachmentInfo);
	}

	const bool hasDepthAttachment = passInfo.depthAttachment.texture != nullptr;
	if (hasDepthAttachment) {
		const SRPassInfo::Attachment& depthAttachment = passInfo.depthAttachment;
		auto internalTexture = to_vk_internal(*depthAttachment.texture);
		assert(internalTexture);

		renderArea.extent.width = std::max(renderArea.extent.width, depthAttachment.texture->info.width);
		renderArea.extent.height = std::max(renderArea.extent.height, depthAttachment.texture->info.height);

		depthAttachmentInfo = {
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = internalTexture->imageView,
			.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			.loadOp = to_vk_load_op(depthAttachment.loadOp),
			.storeOp = to_vk_store_op(depthAttachment.storeOp),
			.clearValue = { .depthStencil = { depthAttachment.clearValue } }
		};
	}

	const VkRenderingInfo renderInfo = {
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

void SRGraphicsDevice_Vulkan::Impl::end_render_pass(const SRSwapchain& swapchain, const SRCmdList& cmdList) {
	auto internalSwapchain = to_vk_internal(swapchain);
	auto internalCmdList = to_vk_internal(cmdList);

	vkCmdEndRendering(internalCmdList->cmdBuffer);

	const SRImageTransitionInfo transitionInfo = {
		.image = internalSwapchain->images[m_ImageIndex],
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

void SRGraphicsDevice_Vulkan::Impl::end_render_pass(const SRCmdList& cmdList) {
	auto internalCmdList = to_vk_internal(cmdList);

	vkCmdEndRendering(internalCmdList->cmdBuffer);
}

void SRGraphicsDevice_Vulkan::Impl::submit_command_lists(const SRSwapchain& swapchain) {
	auto internalSwapchain = to_vk_internal(swapchain);

	const u32 numSubmittedCmdLists = (u32)m_PerFrameCmdListCounters[m_FrameIndex];
	m_PerFrameCmdListCounters[m_FrameIndex] = 0;

	// TODO: Tidy the command buffer submission for different queues to sync.
	// For now we only care about the universal queue
	std::vector<VkCommandBufferSubmitInfo> vkCmdBuffersToSubmit;
	vkCmdBuffersToSubmit.reserve(numSubmittedCmdLists);
	for (u32 i = 0; i < numSubmittedCmdLists; ++i) {
		const SRCmdList_Vulkan* cmdList = m_PerFrameCmdLists[m_FrameIndex][i].get();
		SR_VK_CHECK(vkEndCommandBuffer(cmdList->cmdBuffer), "End command buffer recording");

		const VkCommandBufferSubmitInfo cmdBufferSubmitInfo = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
			.commandBuffer = cmdList->cmdBuffer,
			.deviceMask = 0
		};

		vkCmdBuffersToSubmit.push_back(cmdBufferSubmitInfo);
	}

	const VkSemaphoreSubmitInfo waitSemaphoreInfo = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
		.semaphore = m_ImageAvailableSemaphores[m_FrameIndex],
		.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		.deviceIndex = 0
	};

	const VkSemaphoreSubmitInfo fenceSignalSemaphoreInfo = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
		.semaphore = m_FrameFences[SRQueue_Universal],
		.value = m_NextGPUSignalValue,
		.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
		.deviceIndex = 0
	};

	const VkSemaphoreSubmitInfo renderFinishedSignalSemaphoreInfo = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
		.semaphore = m_RenderFinishedSemaphores[m_FrameIndex],
		.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
		.deviceIndex = 0
	};

	const std::vector<VkSemaphoreSubmitInfo> signalSemaphores = {
		fenceSignalSemaphoreInfo,
		renderFinishedSignalSemaphoreInfo
	};

	const VkSubmitInfo2 submitInfo = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
		.waitSemaphoreInfoCount = 1,
		.pWaitSemaphoreInfos = &waitSemaphoreInfo,
		.commandBufferInfoCount = numSubmittedCmdLists,
		.pCommandBufferInfos = vkCmdBuffersToSubmit.data(),
		.signalSemaphoreInfoCount = static_cast<u32>(signalSemaphores.size()),
		.pSignalSemaphoreInfos = signalSemaphores.data()
	};
	SR_VK_CHECK(vkQueueSubmit2(m_CommandQueues[SRQueue_Universal], 1, &submitInfo, nullptr), "Queue submission");

	const VkPresentInfoKHR presentInfo = {
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &m_RenderFinishedSemaphores[m_FrameIndex],
		.swapchainCount = 1,
		.pSwapchains = &internalSwapchain->swapchain,
		.pImageIndices = &m_ImageIndex
	};
	SR_VK_CHECK(vkQueuePresentKHR(m_CommandQueues[SRQueue_Universal], &presentInfo), "Swapchain present");

	// Await frame value
	m_FrameDoneValue[SRQueue_Universal][m_FrameIndex] = m_NextGPUSignalValue++;
	++m_FrameCounter;
	const u32 nextFrameIndex = (m_FrameIndex + 1) % FRAMES_IN_FLIGHT;

	if (m_FrameCounter >= FRAMES_IN_FLIGHT) {
		u64 needed = m_FrameDoneValue[SRQueue_Universal][nextFrameIndex];
		u64 current = 0;
		SR_VK_CHECK(vkGetSemaphoreCounterValue(m_Device, m_FrameFences[SRQueue_Universal], &current), "Get semaphore counter value");

		if (current < needed) {
			const VkSemaphoreWaitInfo waitInfo = {
				.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
				.semaphoreCount = 1,
				.pSemaphores = &m_FrameFences[SRQueue_Universal],
				.pValues = &needed
			};

			SR_VK_CHECK(vkWaitSemaphores(m_Device, &waitInfo, UINT64_MAX), "Wait for semaphore");
		}
	}

	m_DestructionHandler->update(m_FrameCounter, FRAMES_IN_FLIGHT);
	m_FrameIndex = nextFrameIndex;
}

void SRGraphicsDevice_Vulkan::Impl::draw(u32 vtxCount, u32 startVtx, const SRCmdList& cmdList) {
	auto internalCmdList = to_vk_internal(cmdList);

	vkCmdDraw(internalCmdList->cmdBuffer, vtxCount, 1, startVtx, 0);
}


void SRGraphicsDevice_Vulkan::Impl::draw_indexed(u32 idxCount, u32 startIdx, u32 baseVtx, const SRCmdList& cmdList) {
	auto internalCmdList = to_vk_internal(cmdList);

	vkCmdDrawIndexed(internalCmdList->cmdBuffer, idxCount, 1, startIdx, baseVtx, 0);
}

SRShaderPlatformInfo SRGraphicsDevice_Vulkan::Impl::get_shader_platform_info() {
	return m_ShaderPlatformInfo;
}

void SRGraphicsDevice_Vulkan::Impl::wait_for_gpu() {
	vkDeviceWaitIdle(m_Device);
}

void SRGraphicsDevice_Vulkan::Impl::setup_imgui_init_info(SRFormat swapchainFormat) {
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

	const VkFormat vkSwapchainFormat = to_vk_format(swapchainFormat);
	ImGui_ImplVulkan_InitInfo initInfo = {
		.ApiVersion = VK_API_VERSION_1_4,
		.Instance = m_Instance,
		.PhysicalDevice = m_PhysicalDevice,
		.Device = m_Device,
		.QueueFamily = m_QueueIndices[SRQueue_Universal],
		.Queue = m_CommandQueues[SRQueue_Universal],
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

void SRGraphicsDevice_Vulkan::Impl::flush_initial_uploads() {
	vkEndCommandBuffer(m_UploadCmdBuffer);

	const VkSubmitInfo submitInfo = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount = 1,
		.pCommandBuffers = &m_UploadCmdBuffer
	};

	vkQueueSubmit(m_CommandQueues[SRQueue_Copy], 1, &submitInfo, nullptr);
	vkQueueWaitIdle(m_CommandQueues[SRQueue_Copy]);
	m_IsUploadCmdBufferRecording = false;
}

void SRGraphicsDevice_Vulkan::Impl::populate_debug_messenger_create_info(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
	createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	createInfo.messageSeverity = (
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT
	);
	createInfo.messageType = (
		VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT
	);
	createInfo.pfnUserCallback = debug_callback;
}

VKAPI_ATTR VkBool32 VKAPI_CALL SRGraphicsDevice_Vulkan::Impl::debug_callback(
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

SRDescriptorIndex SRGraphicsDevice_Vulkan::Impl::get_descriptor_index_srv(const SRResource& resource) {
	assert(resource.type == SRResourceType::Texture); // TODO: Support other SRV types

	if (resource.type == SRResourceType::Texture) {
		auto internalTexture = (SRTexture_Vulkan*)resource.internalState.get();
		return internalTexture->srvDescriptor;
	}

	return ~0;
}

// --------------------------------- Public API --------------------------------
SRGraphicsDevice_Vulkan::SRGraphicsDevice_Vulkan(SRWindow& window) : SRGraphicsDevice(window) {
	m_Impl = new Impl(window);
	m_Impl->create_instance();
	m_Impl->create_debug_messenger();
	m_Impl->create_surface();
	m_Impl->create_device();
	m_Impl->create_vulkan_memory_allocator();
	m_Impl->create_command_pools();
	m_Impl->create_sync_objects();
	m_Impl->create_descriptors();
	m_Impl->create_destruction_handler();
}

SRGraphicsDevice_Vulkan::~SRGraphicsDevice_Vulkan() {
	delete m_Impl;
	m_Impl = nullptr;
}

u32 SRGraphicsDevice_Vulkan::get_frame_index() const {
	return m_Impl->m_FrameIndex;
}

void SRGraphicsDevice_Vulkan::create_swapchain(const SRSwapchainInfo& info, SRSwapchain& swapchain) {
	m_Impl->create_swapchain(info, swapchain);
}

void SRGraphicsDevice_Vulkan::create_pipeline(const SRPipelineInfo& info, SRPipeline& pipeline) {
	m_Impl->create_pipeline(info, pipeline);
}

void SRGraphicsDevice_Vulkan::create_buffer(const SRBufferInfo& info, SRBuffer& buffer, const void* data) {
	m_Impl->create_buffer(info, buffer, data);
}

void SRGraphicsDevice_Vulkan::create_texture(const SRTextureInfo& info, SRTexture& texture, const SRSubresourceData* data) {
	m_Impl->create_texture(info, texture, data);
}

void SRGraphicsDevice_Vulkan::create_sampler(const SRSamplerInfo& info, SRSampler& sampler) {
	m_Impl->create_sampler(info, sampler);
}

void SRGraphicsDevice_Vulkan::bind_pipeline(const SRPipeline& pipeline, const SRCmdList& cmdList) {
	m_Impl->bind_pipeline(pipeline, cmdList);
}

void SRGraphicsDevice_Vulkan::bind_viewport(const SRViewport& viewport, const SRCmdList& cmdList) {
	auto internalCmdList = to_vk_internal(cmdList);

	// We need to flip the viewport vertically in order to work with DX12
	const VkViewport vkViewport = {
		.x = viewport.topLeftX,
		.y = viewport.topLeftY + viewport.height,
		.width = viewport.width,
		.height = -viewport.height,
		.minDepth = viewport.minDepth,
		.maxDepth = viewport.maxDepth
	};

	const VkExtent2D scissorExtent = {
		.width = static_cast<u32>(viewport.width),
		.height = static_cast<u32>(viewport.height)
	};


	const VkRect2D scissor = {
		.offset = { 0, 0 },
		.extent = scissorExtent
	};

	vkCmdSetViewport(internalCmdList->cmdBuffer, 0, 1, &vkViewport);
	vkCmdSetScissor(internalCmdList->cmdBuffer, 0, 1, &scissor);
}

void SRGraphicsDevice_Vulkan::bind_vertex_buffer(const SRBuffer& buffer, const SRCmdList& cmdList) {
	m_Impl->bind_vertex_buffer(buffer, cmdList);
}

void SRGraphicsDevice_Vulkan::bind_index_buffer(const SRBuffer& buffer, const SRCmdList& cmdList) {
	m_Impl->bind_index_buffer(buffer, cmdList);
}

void SRGraphicsDevice_Vulkan::bind_root_constant_buffer(const SRBuffer& buffer, const SRCmdList& cmdList) {
	m_Impl->bind_root_constant_buffer(buffer, cmdList);
}

void SRGraphicsDevice_Vulkan::push_constants(const void* data, u32 size, const SRCmdList& cmdList) {
	m_Impl->push_constants(data, size, cmdList);
}

void SRGraphicsDevice_Vulkan::barrier(const SRBarrier* pBarriers, u32 numBarriers, const SRCmdList& cmdList) {
	m_Impl->barrier(pBarriers, numBarriers, cmdList);
}

SRCmdList SRGraphicsDevice_Vulkan::begin_command_list(SRQueue queue) {
	return m_Impl->begin_command_list(queue);
}

void SRGraphicsDevice_Vulkan::begin_render_pass(const SRSwapchain& swapchain, const SRCmdList& cmdList) {
	m_Impl->begin_render_pass(swapchain, cmdList);
}

void SRGraphicsDevice_Vulkan::begin_render_pass(const SRPassInfo& passInfo, const SRCmdList& cmdList) {
	m_Impl->begin_render_pass(passInfo, cmdList);
}

void SRGraphicsDevice_Vulkan::end_render_pass(const SRSwapchain& swapchain, const SRCmdList& cmdList) {
	m_Impl->end_render_pass(swapchain, cmdList);
}

void SRGraphicsDevice_Vulkan::end_render_pass(const SRCmdList& cmdList) {
	m_Impl->end_render_pass(cmdList);
}

void SRGraphicsDevice_Vulkan::submit_command_lists(const SRSwapchain& swapchain) {
	m_Impl->submit_command_lists(swapchain);
}

void SRGraphicsDevice_Vulkan::draw(u32 vtxCount, u32 startVtx, const SRCmdList& cmdList) {
	m_Impl->draw(vtxCount, startVtx, cmdList);
}

void SRGraphicsDevice_Vulkan::draw_indexed(u32 idxCount, u32 startIdx, u32 baseVtx, const SRCmdList& cmdList) {
	m_Impl->draw_indexed(idxCount, startIdx, baseVtx, cmdList);
}

void SRGraphicsDevice_Vulkan::dispatch_mesh(u32 groupCountX, u32 groupCountY, u32 groupCountZ, const SRCmdList& cmdList) {

}

SRDescriptorIndex SRGraphicsDevice_Vulkan::get_descriptor_index_srv(const SRResource& resource) {
	return m_Impl->get_descriptor_index_srv(resource);
}

SRShaderPlatformInfo SRGraphicsDevice_Vulkan::get_shader_platform_info() {
	return m_Impl->get_shader_platform_info();
}

void SRGraphicsDevice_Vulkan::wait_for_gpu() {
	m_Impl->wait_for_gpu();
}

void SRGraphicsDevice_Vulkan::flush_initial_uploads() {
	m_Impl->flush_initial_uploads();
}

void SRGraphicsDevice_Vulkan::setup_imgui_init_info(SRFormat swapchainFormat) {
	m_Impl->setup_imgui_init_info(swapchainFormat);
}
