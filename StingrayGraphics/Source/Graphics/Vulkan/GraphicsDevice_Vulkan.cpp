#define VMA_IMPLEMENTATION

#include "GraphicsDevice_Vulkan.hpp"
#include "Graphics/GraphicsTypes.hpp"
#include "Graphics/Vulkan/GraphicsHelpers_Vulkan.hpp"
#include "Graphics/Vulkan/GraphicsTypes_Vulkan.hpp"
#include "Core/Logger.hpp"
#include "Core/System/MonitorEnumerator.hpp"

#include <Windows.h>

#include <algorithm>
#include <cassert>
#include <memory>
#include <vector>
#include <stdexcept>

namespace {
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
		VK_EXT_MESH_SHADER_EXTENSION_NAME,
		VK_KHR_SPIRV_1_4_EXTENSION_NAME
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
	uint32_t m_QueueIndices[SRQueue_COUNT] = {};
	VkSemaphore m_FrameFences[SRQueue_COUNT] = {};
	VkSemaphore m_ImageAvailableSemaphores[FRAMES_IN_FLIGHT] = {};
	VkSemaphore m_RenderFinishedSemaphores[FRAMES_IN_FLIGHT] = {};
	VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
	VkDescriptorSet m_ResourceDescriptorSet = VK_NULL_HANDLE;
	VkDescriptorSetLayout m_ResourceDescriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorSetLayout m_PushDescriptorSetLayout = VK_NULL_HANDLE;
	SRDescriptorHeap_Vulkan m_TextureDescriptorHeap = { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, MAX_TEXTURE_DESCRIPTORS };
	SRDescriptorHeap_Vulkan m_SamplerDescriptorHeap = { VK_DESCRIPTOR_TYPE_SAMPLER, MAX_SAMPLER_DESCRIPTORS };
	SRDescriptorHeap_Vulkan m_StorageBufferDescriptorHeap = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, MAX_STORAGE_BUFFER_DESCRIPTORS };
	SRDescriptorHeap_Vulkan m_RWTextureDescriptorHeap = { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, MAX_RW_TEXTURE_DESCRIPTORS };
	SRPipeline_Vulkan* m_ActivePipeline = nullptr;
	std::unique_ptr<SRDestructionHandler_Vulkan> m_DestructionHandler;

	uint64_t m_NextGPUSignalValue = 1;
	uint64_t m_FrameDoneValue[SRQueue_COUNT][FRAMES_IN_FLIGHT] = {};
	std::vector<std::unique_ptr<SRCmdList_Vulkan>> m_PerFrameCmdLists[FRAMES_IN_FLIGHT];
	size_t m_PerFrameCmdListCounters[FRAMES_IN_FLIGHT] = {};
	uint32_t m_FrameIndex = 0;
	uint32_t m_ImageIndex = 0;
	uint64_t m_FrameCounter = 0;

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

	void bind_pipeline(const SRPipeline& pipeline, const SRCmdList& cmdList);
	void bind_vertex_buffer(const SRBuffer& buffer, const SRCmdList& cmdList);
	void bind_index_buffer(const SRBuffer& buffer, const SRCmdList& cmdList);
	void bind_root_constant_buffer(const SRBuffer& buffer, const SRCmdList& cmdList);

	SRCmdList begin_command_list(SRQueue queue);
	void begin_render_pass(const SRSwapchain& swapchain, const SRCmdList& cmdList);
	void end_render_pass(const SRSwapchain& swapchain, const SRCmdList& cmdList);
	void submit_command_lists(const SRSwapchain& swapchain);

	void draw(uint32_t vtxCount, uint32_t startVtx, const SRCmdList& cmdList);
	void draw_indexed(uint32_t idxCount, uint32_t startIdx, uint32_t baseVtx, const SRCmdList& cmdList);

	SRShaderPlatformInfo get_shader_platform_info();
	void wait_for_gpu();

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
	static constexpr uint32_t MAX_UNIFORM_BUFFER_DESCRIPTORS = 64;
	static constexpr uint32_t MAX_TEXTURE_DESCRIPTORS = 16384;
	static constexpr uint32_t MAX_SAMPLER_DESCRIPTORS = 32;
	static constexpr uint32_t MAX_RW_TEXTURE_DESCRIPTORS = 16384;
	static constexpr uint32_t MAX_STORAGE_BUFFER_DESCRIPTORS = 2048;
	static constexpr uint32_t UBO_BINDING = 0;
	static constexpr uint32_t UBO_SET = 1;
	static constexpr uint32_t TEXTURE_BINDING = 1;
	static constexpr uint32_t SAMPLER_BINDING = 2;
	static constexpr uint32_t STORAGE_BUFFER_BINDING = 3;
	static constexpr uint32_t RW_TEXTURE_BINDING = 4;
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
	for (uint32_t q = 0; q < SRQueue_COUNT; ++q) {
		for (uint32_t f = 0; f < FRAMES_IN_FLIGHT; ++f) {
			m_DestructionHandler->enqueue(m_CommandPools[q][f]);
		}
	}
	m_DestructionHandler->enqueue(m_UploadCmdPool);

	// Fences (timeline semaphores)
	for (uint32_t q = 0; q < SRQueue_COUNT; ++q) {
		m_DestructionHandler->enqueue(m_FrameFences[q]);
	}

	// Semaphores
	for (uint32_t f = 0; f < FRAMES_IN_FLIGHT; f++) {
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
	uint32_t numInstanceLayers;
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
	uint32_t numInstanceExts;
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

	instanceInfo.enabledExtensionCount = static_cast<uint32_t>(enabledExts.size());
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
	uint32_t numDevices = 0;
	SR_VK_CHECK(vkEnumeratePhysicalDevices(m_Instance, &numDevices, nullptr), "Physical device enumeration");

	if (numDevices == 0) {
		SRLOG_CRITICAL_CAT(SRLOG_CAT_VULKAN, "No GPU with Vulkan support was found");
		throw std::runtime_error("Vulkan error: No GPU with Vulkan support was found");
	}

	std::vector<VkPhysicalDevice> devices(numDevices);
	SR_VK_CHECK(vkEnumeratePhysicalDevices(m_Instance, &numDevices, devices.data()), "Physical device enumeration");

	SRLOG_DEBUG_CAT(SRLOG_CAT_VULKAN, "Found %u potential device(s). Enumerating...", numDevices);
	uint32_t pickedDeviceIdx = ~0;
	const char* deviceName = nullptr;
	for (uint32_t i = 0; i < numDevices; ++i) {
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
		uint32_t numDeviceExts;
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
		VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeatures = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
			.pNext = nullptr
		};
		VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeatures = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
			.pNext = &rtPipelineFeatures,
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
			.pNext = &vk12Features,
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

		// Set enabled device features
		VkPhysicalDeviceRayTracingPipelineFeaturesKHR enableRTFeat = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
			.pNext = nullptr,
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
		uint32_t numQueueFamilies;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &numQueueFamilies, nullptr);
		std::vector<VkQueueFamilyProperties> queueFamilies(numQueueFamilies);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &numQueueFamilies, queueFamilies.data());

		uint32_t universalQueueFamilyIdx = ~0; // Graphics + Compute + Copy (REQUIRED)
		uint32_t dedicatedComputeQueueFamilyIdx = ~0; // REQUIRED
		uint32_t dedicatedCopyQueueFamilyIdx = ~0; // OPTIONAL, though might be made REQUIRED in the future

		for (uint32_t i = 0; i < numQueueFamilies; ++i) {
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
			.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
			.pQueueCreateInfos = queueCreateInfos.data(),
			.enabledExtensionCount = static_cast<uint32_t>(enabledExts.size()),
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

	for (uint32_t q = 0; q < SRQueue_COUNT; ++q) {
		poolInfo.queueFamilyIndex = m_QueueIndices[q];

		for (uint32_t f = 0; f < FRAMES_IN_FLIGHT; ++f) {
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

	for (uint32_t q = 0; q < SRQueue_COUNT; ++q) {
		SR_VK_CHECK(vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &(m_FrameFences[q])), "Timeline semaphore creation");
	}

	// Image available and render finished semaphores
	semaphoreInfo.pNext = nullptr;
	for (uint32_t f = 0; f < FRAMES_IN_FLIGHT; ++f) {
		SR_VK_CHECK(vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &m_ImageAvailableSemaphores[f]), "Image-available semaphore creation");
		SR_VK_CHECK(vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &m_RenderFinishedSemaphores[f]), "Render-finished semaphore creation");
	}
}

void SRGraphicsDevice_Vulkan::Impl::create_descriptors() {
	// Bindless descriptors (set 0)
	std::vector<SRDescriptorHeap_Vulkan*> descriptorHeaps = {
		&m_TextureDescriptorHeap,
		&m_SamplerDescriptorHeap,
		&m_StorageBufferDescriptorHeap,
		&m_RWTextureDescriptorHeap
	};
	std::vector<VkDescriptorPoolSize> poolSizes;
	std::vector<VkDescriptorBindingFlags> bindingFlags;
	std::vector<VkDescriptorSetLayoutBinding> layoutBindings;
	poolSizes.reserve(descriptorHeaps.size());
	bindingFlags.reserve(descriptorHeaps.size());
	layoutBindings.reserve(descriptorHeaps.size());

	for (size_t i = 0; i < descriptorHeaps.size(); ++i) {
		const SRDescriptorHeap_Vulkan* heap = descriptorHeaps[i];
		const VkDescriptorType descriptorType = heap->get_type();
		const uint32_t descriptorCount = heap->get_capacity();

		const VkDescriptorPoolSize poolSize = { descriptorType, descriptorCount };
		const VkDescriptorBindingFlags flags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
		const VkDescriptorSetLayoutBinding layoutBinding = {
			.binding = static_cast<uint32_t>(i),
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
	const VkDescriptorPoolCreateInfo poolInfo = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
		.maxSets = 1,
		.poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
		.pPoolSizes = poolSizes.data()
	};
	SR_VK_CHECK(vkCreateDescriptorPool(m_Device, &poolInfo, nullptr, &m_DescriptorPool), "Create descriptor pool");

	// Descriptor set layout
	const VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
		.bindingCount = static_cast<uint32_t>(bindingFlags.size()),
		.pBindingFlags = bindingFlags.data()
	};
	const VkDescriptorSetLayoutCreateInfo setLayoutInfo = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.pNext = &bindingFlagsInfo,
		.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
		.bindingCount = static_cast<uint32_t>(layoutBindings.size()),
		.pBindings = layoutBindings.data()
	};
	SR_VK_CHECK(vkCreateDescriptorSetLayout(m_Device, &setLayoutInfo, nullptr, &m_ResourceDescriptorSetLayout), "Create descriptor set layout");

	// Descriptor set
	const VkDescriptorSetAllocateInfo descriptorSetAllocInfo = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = m_DescriptorPool,
		.descriptorSetCount = 1,
		.pSetLayouts = &m_ResourceDescriptorSetLayout
	};
	SR_VK_CHECK(vkAllocateDescriptorSets(m_Device, &descriptorSetAllocInfo, &m_ResourceDescriptorSet), "Allocate descriptor sets");

	// Push descriptor
	const VkDescriptorSetLayoutBinding uboBinding = {
		.binding = UBO_BINDING,
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_ALL,
		.pImmutableSamplers = nullptr
	};

	const VkDescriptorSetLayoutCreateInfo pushLayoutInfo = {
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
	if (supportInfo.capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
		extent = supportInfo.capabilities.currentExtent;
	}
	else {
		int width;
		int height;
		m_Window.get_client_size(&width, &height);
		extent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };

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
		.minImageCount = static_cast<uint32_t>(info.numBuffers),
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
	uint32_t numImages;
	SR_VK_CHECK(vkGetSwapchainImagesKHR(m_Device, internalSwapchain->swapchain, &numImages, nullptr), "Get swapchain images");
	internalSwapchain->images.resize(numImages);
	internalSwapchain->imageViews.resize(numImages);
	SR_VK_CHECK(vkGetSwapchainImagesKHR(m_Device, internalSwapchain->swapchain, &numImages, internalSwapchain->images.data()), "Get swapchain images");

	// Swapchain image views
	for (uint32_t i = 0; i < numImages; ++i) {
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
			.pName = "main",
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
			.pName = "main",
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
		.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
		.pDynamicStates = dynamicStates.data()
	};

	// Attribute and binding descriptions
	std::vector<VkVertexInputAttributeDescription> attributeDescriptions(info.inputLayout.elements.size());
	uint32_t offset = 0;

	for (size_t i = 0; i < attributeDescriptions.size(); i++) {
		attributeDescriptions[i].binding = 0;
		attributeDescriptions[i].location = static_cast<uint32_t>(i); // TODO: Doesn't work for all formats
		attributeDescriptions[i].format = to_vk_format(info.inputLayout.elements[i].format);
		attributeDescriptions[i].offset = offset;

		offset += SRVulkanHelpers::get_format_stride(info.inputLayout.elements[i].format);
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
		.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size()),
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
	for (uint32_t i = 0; i < info.numRenderTargets; ++i) {
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
		.attachmentCount = static_cast<uint32_t>(colorBlendStates.size()),
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
		m_PushDescriptorSetLayout
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
		.depthWriteEnable = info.depthStencilState.depthWriteMask == SRDepthWriteMask::ZERO ? VK_FALSE : VK_TRUE,
		.depthCompareOp = to_vk_comparison_func(info.depthStencilState.depthFunction),
		.depthBoundsTestEnable = VK_FALSE,
		.stencilTestEnable = VK_FALSE,
		.minDepthBounds = 0.0f,
		.maxDepthBounds = 1.0f
	};

	const VkGraphicsPipelineCreateInfo pipelineInfo = {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.pNext = &pipelineRenderingInfo,
		.stageCount = static_cast<uint32_t>(shaderStages.size()),
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

	if (info.bindFlags & SRBindFlag_VertexBuffer) {
		createInfo.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	}
	else if (info.bindFlags & SRBindFlag_IndexBuffer) {
		createInfo.usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	}
	else if (info.bindFlags & SRBindFlag_ConstantBuffer) {
		createInfo.usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	}
	if (info.miscFlags & SRMiscFlag_StructuredBuffer) {
		createInfo.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	}

	switch (info.usage) {
	case SRUsage::UPLOAD:
		allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
		break;
	}
	//SR_VK_CHECK(vkCreateBuffer(m_Device, &createInfo, nullptr, &internalBuffer->buffer), "Create buffer");

	VmaAllocationInfo allocInfo = {};
	SR_VK_CHECK(vmaCreateBuffer(
		m_Allocator,
		&createInfo,
		&allocCreateInfo,
		&internalBuffer->buffer,
		&internalBuffer->allocation,
		&allocInfo
	), "Create buffer");

	if (info.usage == SRUsage::DEFAULT && data != nullptr) {
		// Staging buffer
		SRBufferInfo stagingBufferInfo = info;
		stagingBufferInfo.usage = SRUsage::UPLOAD;
		stagingBufferInfo.bindFlags = SRBindFlag_None;
		stagingBufferInfo.miscFlags = SRMiscFlag_None;

		SRBuffer stagingBuffer;
		create_buffer(stagingBufferInfo, stagingBuffer, data);
		auto internalStagingBuffer = to_internal(stagingBuffer);

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
	else if (info.usage == SRUsage::UPLOAD) {
		buffer.mappedData = internalBuffer->allocation->GetMappedData();
		buffer.mappedSize = info.size;

		if (data != nullptr) {
			std::memcpy(buffer.mappedData, data, info.size);
		}
	}

	// TODO: Descriptors (non UBO that is)
}

void SRGraphicsDevice_Vulkan::Impl::bind_pipeline(const SRPipeline& pipeline, const SRCmdList& cmdList) {
	auto internalPipeline = to_internal(pipeline);
	auto internalCmdList = to_internal(cmdList);

	vkCmdBindPipeline(internalCmdList->cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, internalPipeline->pipeline);
	m_ActivePipeline = internalPipeline;
}

void SRGraphicsDevice_Vulkan::Impl::bind_vertex_buffer(const SRBuffer& buffer, const SRCmdList& cmdList) {
	assert(buffer.info.bindFlags & SRBindFlag_VertexBuffer);
	auto internalBuffer = to_internal(buffer);
	auto internalCmdList = to_internal(cmdList);

	const VkDeviceSize offset = 0;
	vkCmdBindVertexBuffers(internalCmdList->cmdBuffer, 0, 1, &internalBuffer->buffer, &offset);
}

void SRGraphicsDevice_Vulkan::Impl::bind_index_buffer(const SRBuffer& buffer, const SRCmdList& cmdList) {
	assert(buffer.info.bindFlags & SRBindFlag_IndexBuffer);
	auto internalBuffer = to_internal(buffer);
	auto internalCmdList = to_internal(cmdList);

	vkCmdBindIndexBuffer(internalCmdList->cmdBuffer, internalBuffer->buffer, 0, VK_INDEX_TYPE_UINT32);
}

void SRGraphicsDevice_Vulkan::Impl::bind_root_constant_buffer(const SRBuffer& buffer, const SRCmdList& cmdList) {
	assert(buffer.info.bindFlags & SRBindFlag_ConstantBuffer);
	assert(m_ActivePipeline != nullptr);

	auto internalBuffer = to_internal(buffer);
	auto internalCmdList = to_internal(cmdList);

	const VkDescriptorBufferInfo bufferInfo = {
		.buffer = internalBuffer->buffer,
		.offset = 0,
		.range = buffer.info.size
	};
	const VkWriteDescriptorSet writeDescriptor = {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = VK_NULL_HANDLE,
		.dstBinding = UBO_BINDING,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.pBufferInfo = &bufferInfo,
	};

	vkCmdPushDescriptorSet(
		internalCmdList->cmdBuffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		m_ActivePipeline->pipelineLayout,
		UBO_SET,
		1,
		&writeDescriptor
	);
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
	auto internalSwapchain = to_internal(swapchain);
	auto internalCmdList = to_internal(cmdList);

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

void SRGraphicsDevice_Vulkan::Impl::end_render_pass(const SRSwapchain& swapchain, const SRCmdList& cmdList) {
	auto internalSwapchain = to_internal(swapchain);
	auto internalCmdList = to_internal(cmdList);

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

void SRGraphicsDevice_Vulkan::Impl::submit_command_lists(const SRSwapchain& swapchain) {
	auto internalSwapchain = to_internal(swapchain);

	const uint32_t numSubmittedCmdLists = m_PerFrameCmdListCounters[m_FrameIndex];
	m_PerFrameCmdListCounters[m_FrameIndex] = 0;

	// TODO: Tidy the command buffer submission for different queues to sync.
	// For now we only care about the universal queue
	std::vector<VkCommandBufferSubmitInfo> vkCmdBuffersToSubmit;
	vkCmdBuffersToSubmit.reserve(numSubmittedCmdLists);
	for (uint32_t i = 0; i < numSubmittedCmdLists; ++i) {
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
		.signalSemaphoreInfoCount = static_cast<uint32_t>(signalSemaphores.size()),
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
	const uint32_t nextFrameIndex = (m_FrameIndex + 1) % FRAMES_IN_FLIGHT;

	if (m_FrameCounter >= FRAMES_IN_FLIGHT) {
		const uint64_t needed = m_FrameDoneValue[SRQueue_Universal][nextFrameIndex];
		uint64_t current = 0;
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

void SRGraphicsDevice_Vulkan::Impl::draw(uint32_t vtxCount, uint32_t startVtx, const SRCmdList& cmdList) {
	auto internalCmdList = to_internal(cmdList);

	vkCmdDraw(internalCmdList->cmdBuffer, vtxCount, 1, startVtx, 0);
}


void SRGraphicsDevice_Vulkan::Impl::draw_indexed(uint32_t idxCount, uint32_t startIdx, uint32_t baseVtx, const SRCmdList& cmdList) {
	auto internalCmdList = to_internal(cmdList);

	vkCmdDrawIndexed(internalCmdList->cmdBuffer, idxCount, 1, startIdx, baseVtx, 0);
}

SRShaderPlatformInfo SRGraphicsDevice_Vulkan::Impl::get_shader_platform_info() {
	return m_ShaderPlatformInfo;
}

void SRGraphicsDevice_Vulkan::Impl::wait_for_gpu() {
	vkDeviceWaitIdle(m_Device);
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

uint32_t SRGraphicsDevice_Vulkan::get_frame_index() const {
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

void SRGraphicsDevice_Vulkan::bind_pipeline(const SRPipeline& pipeline, const SRCmdList& cmdList) {
	m_Impl->bind_pipeline(pipeline, cmdList);
}

void SRGraphicsDevice_Vulkan::bind_viewport(const SRViewport& viewport, const SRCmdList& cmdList) {
	auto internalCmdList = to_internal(cmdList);

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
		.width = static_cast<uint32_t>(viewport.width),
		.height = static_cast<uint32_t>(viewport.height)
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

SRCmdList SRGraphicsDevice_Vulkan::begin_command_list(SRQueue queue) {
	return m_Impl->begin_command_list(queue);
}

void SRGraphicsDevice_Vulkan::begin_render_pass(const SRSwapchain& swapchain, const SRCmdList& cmdList) {
	m_Impl->begin_render_pass(swapchain, cmdList);
}

void SRGraphicsDevice_Vulkan::end_render_pass(const SRSwapchain& swapchain, const SRCmdList& cmdList) {
	m_Impl->end_render_pass(swapchain, cmdList);
}

void SRGraphicsDevice_Vulkan::submit_command_lists(const SRSwapchain& swapchain) {
	m_Impl->submit_command_lists(swapchain);
}

void SRGraphicsDevice_Vulkan::draw(uint32_t vtxCount, uint32_t startVtx, const SRCmdList& cmdList) {
	m_Impl->draw(vtxCount, startVtx, cmdList);
}

void SRGraphicsDevice_Vulkan::draw_indexed(uint32_t idxCount, uint32_t startIdx, uint32_t baseVtx, const SRCmdList& cmdList) {
	m_Impl->draw_indexed(idxCount, startIdx, baseVtx, cmdList);
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
