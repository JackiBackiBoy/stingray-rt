#include "gfx_device_vulkan.h"

#include "gfx_types_vulkan.h"
#include "../../platform.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <deque>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <iostream>
#include <set>
#include <vector>

// Vulkan helpers
namespace vk_helpers {
	struct ImageTransitionInfo {
		VkImage image = nullptr;
		VkImageLayout oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		VkImageLayout newLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		VkAccessFlags2 srcAccessMask = VK_ACCESS_2_NONE;
		VkAccessFlags2 dstAccessMask = VK_ACCESS_2_NONE;
		VkPipelineStageFlags2 srcStageMask = VK_PIPELINE_STAGE_2_NONE;
		VkPipelineStageFlags2 dstStageMask = VK_PIPELINE_STAGE_2_NONE;
	};

	void transition_image_layout(const ImageTransitionInfo& info, VkCommandBuffer commandBuffer) {
		// TODO: Doesn't work for depth attachments
		const VkImageSubresourceRange subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1
		};

		const VkImageMemoryBarrier2 imageBarrier = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.pNext = nullptr,
			.srcStageMask = info.srcStageMask,
			.srcAccessMask = info.srcAccessMask,
			.dstStageMask = info.dstStageMask,
			.dstAccessMask = info.dstAccessMask,
			.oldLayout = info.oldLayout,
			.newLayout = info.newLayout,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = info.image,
			.subresourceRange = subresourceRange
		};

		const VkDependencyInfo dependencyInfo = {
			.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			.pNext = nullptr,
			.imageMemoryBarrierCount = 1,
			.pImageMemoryBarriers = &imageBarrier
		};

		vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);
	}
}

// GFXDevice_Vulkan Implemenation
struct GFXDevice_Vulkan::Impl {
	Impl(GLFWwindow* window);
	~Impl();

	struct Descriptor {
		uint32_t index = 0;

		void init_ubo(Impl* impl, VkBuffer buffer);
	};

	// NOTE: In Vulkan, we do not need separate command pools
	// like in DX12 where we need different descriptor heap types.
	// However, we use a "DescriptorHeap" structure here to allow
	// for simpler descriptor indexing lookup based on descriptor
	// type. I.e. some resource might use different "DescriptorHeap"
	// but they will all be allocated from the same VkCommandPool
	class DescriptorHeap {
	public:
		DescriptorHeap(uint32_t capacity) : m_Capacity(capacity) {}

		inline uint32_t getCurrentDescriptorHandle() const { return m_CurrentDescriptorHandle; }
		inline void offsetCurrentDescriptorHandle(uint32_t offset) { m_CurrentDescriptorHandle += offset; }

	private:
		uint32_t m_CurrentDescriptorHandle = 0;
		uint32_t m_Capacity = 0;
	};

	struct Buffer_Vulkan {
		~Buffer_Vulkan() {
			const uint64_t frameCount = destructionHandler->frameCount;
			destructionHandler->buffers.push_back({ buffer, frameCount });
			destructionHandler->allocations.push_back({ bufferMemory, frameCount });
		}

		DestructionHandler* destructionHandler = nullptr;
		Descriptor descriptor = {};
		BufferInfo info = {};
		VkBuffer buffer = nullptr;
		VkDeviceMemory bufferMemory = nullptr;
	};

	Buffer_Vulkan* to_internal(const Buffer& buffer) {
		return (Buffer_Vulkan*)buffer.internalState.get();
	}

	struct QueueFamilyIndices {
		std::optional<uint32_t> graphicsFamily;
		std::optional<uint32_t> presentFamily;

		inline bool is_complete() {
			return graphicsFamily.has_value() && presentFamily.has_value();
		}
	};

	struct SwapChainSupportInfo {
		VkSurfaceCapabilitiesKHR capabilities = {};
		std::vector<VkSurfaceFormatKHR> formats = {};
		std::vector<VkPresentModeKHR> presentModes = {};
	};

	void create_internal_swapchain(SwapChain_Vulkan* internalState);
	void create_instance();
	void create_debug_messenger();
	void create_surface();
	void pick_physical_device();
	void create_device();
	void create_destruction_handler();
	void create_command_pool();
	void create_sync_objects();
	void create_descriptors();

	bool check_validation_layers();
	bool is_device_suitable(VkPhysicalDevice device);
	std::vector<const char*> get_required_extensions();
	static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData);
	static VkResult create_debug_utils_messenger(
		VkInstance instance,
		const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
		const VkAllocationCallbacks* pAllocator,
		VkDebugUtilsMessengerEXT* pDebugMessenger);
	static void destroy_debug_utils_messenger(
		VkInstance instance,
		VkDebugUtilsMessengerEXT debugMessenger,
		const VkAllocationCallbacks* pAllocator);
	static void populate_debug_messenger_create_info(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
	bool check_device_extension_support(VkPhysicalDevice device);

	QueueFamilyIndices find_queue_families(VkPhysicalDevice device);
	uint32_t find_memory_type(uint32_t typeFilter, VkMemoryPropertyFlags properties);
	SwapChainSupportInfo query_swapchain_support(VkPhysicalDevice device);

	DestructionHandler m_DestructionHandler = {};
	GLFWwindow* m_Window = nullptr;
	VkInstance m_Instance = nullptr;
	VkPhysicalDevice m_PhysicalDevice = nullptr;
	VkDevice m_Device = nullptr;
	VkQueue m_GraphicsQueue = nullptr;
	VkQueue m_PresentQueue = nullptr;
	Pipeline_Vulkan* m_ActivePipeline = nullptr;

	VkCommandPool m_CommandPool = nullptr;
	std::vector<std::unique_ptr<CommandList_Vulkan>> m_CmdLists = {};
	size_t m_CmdListCounter = 0;

	// Descriptors
	VkDescriptorPool m_DescriptorPool = nullptr;
	VkDescriptorSet m_ResourceDescriptorSet = nullptr;
	VkDescriptorSetLayout m_ResourceDescriptorSetLayout = nullptr;
	DescriptorHeap m_BufferDescriptorHeap = DescriptorHeap(32);

	// Synchronization
	std::vector<VkSemaphore> m_ImageAvailableSemaphores = {};
	std::vector<VkSemaphore> m_RenderFinishedSemaphores = {};
	std::vector<VkFence> m_InFlightFences = {};

	VkSurfaceKHR m_Surface = nullptr;
	VkDebugUtilsMessengerEXT m_DebugMessenger = nullptr;

	const std::vector<const char*> m_ValidationLayers = {
		"VK_LAYER_KHRONOS_validation"
	};

	const std::vector<const char*> m_DeviceExtensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
	};

	#ifdef NDEBUG
		static constexpr bool m_EnableValidationLayers = false;
	#else
		static constexpr bool m_EnableValidationLayers = true;
	#endif
};

void GFXDevice_Vulkan::Impl::Descriptor::init_ubo(Impl* impl, VkBuffer buffer) {
	const VkDescriptorBufferInfo descriptorBufferInfo = {
		.buffer = buffer,
		.offset = 0,
		.range = VK_WHOLE_SIZE
	};

	// TODO: Perhaps accumulate descriptor writes instead of one at a time?
	index = impl->m_BufferDescriptorHeap.getCurrentDescriptorHandle();
	impl->m_BufferDescriptorHeap.offsetCurrentDescriptorHandle(1);

	const VkWriteDescriptorSet descriptorWrite = {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = impl->m_ResourceDescriptorSet,
		.dstBinding = 0, // TODO: Remove hardcoded value
		.dstArrayElement = index,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.pBufferInfo = &descriptorBufferInfo
	};

	vkUpdateDescriptorSets(impl->m_Device, 1, &descriptorWrite, 0, nullptr);
}

GFXDevice_Vulkan::Impl::Impl(GLFWwindow* window) : m_Window(window) {
	create_instance();
	create_debug_messenger();
	create_surface();
	pick_physical_device();
	create_device();
	create_destruction_handler();
	create_command_pool();
	create_sync_objects();
	create_descriptors();
}

GFXDevice_Vulkan::Impl::~Impl() {
	if constexpr (m_EnableValidationLayers) {
		destroy_debug_utils_messenger(m_Instance, m_DebugMessenger, nullptr);
	}

	const uint64_t frameCount = m_DestructionHandler.frameCount;

	m_DestructionHandler.surfaces.push_back({ m_Surface, frameCount });
	m_DestructionHandler.commandPools.push_back({ m_CommandPool, frameCount });

	for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
		m_DestructionHandler.semaphores.push_back({ m_ImageAvailableSemaphores[i], frameCount });
		m_DestructionHandler.semaphores.push_back({ m_RenderFinishedSemaphores[i], frameCount });
		m_DestructionHandler.fences.push_back({ m_InFlightFences[i], frameCount });
	}
}

void GFXDevice_Vulkan::Impl::create_instance() {
	if (m_EnableValidationLayers && !check_validation_layers()) {
		throw std::runtime_error("VULKAN ERROR: Validation layers not available!");
	}

	const VkApplicationInfo appInfo = {
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pNext = nullptr,
		.pApplicationName = "Engine",
		.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),
		.pEngineName = "Engine",
		.engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),
		.apiVersion = VK_API_VERSION_1_3
	};

	VkInstanceCreateInfo createInfo = {
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.flags = 0,
		.pApplicationInfo = &appInfo,
	};

	// Validation layers
	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {};
	if constexpr (m_EnableValidationLayers) {
		populate_debug_messenger_create_info(debugCreateInfo);
		createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;

		createInfo.enabledLayerCount = static_cast<uint32_t>(m_ValidationLayers.size());
		createInfo.ppEnabledLayerNames = m_ValidationLayers.data();
	}
	else {
		createInfo.pNext = nullptr;
		createInfo.enabledLayerCount = 0;
		createInfo.ppEnabledLayerNames = nullptr;
	}

	// Vulkan extensions
	const auto extensions = get_required_extensions();
	createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	createInfo.ppEnabledExtensionNames = extensions.data();

	VkResult result = vkCreateInstance(&createInfo, nullptr, &m_Instance);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("VULKAN ERROR: Failed to create Vulkan instance!");
	}
}

void GFXDevice_Vulkan::Impl::create_debug_messenger() {
	if constexpr (!m_EnableValidationLayers) {
		return;
	}

	VkDebugUtilsMessengerCreateInfoEXT createInfo;
	populate_debug_messenger_create_info(createInfo);

	if (create_debug_utils_messenger(m_Instance, &createInfo, nullptr, &m_DebugMessenger) != VK_SUCCESS) {
		throw std::runtime_error("failed to set up debug messenger!");
	}
}

void GFXDevice_Vulkan::Impl::create_surface() {
	if (glfwCreateWindowSurface(m_Instance, m_Window, nullptr, &m_Surface) != VK_SUCCESS) {
		throw std::runtime_error("VULKAN ERROR: Failed to create window surface!");
	}
}

void GFXDevice_Vulkan::Impl::pick_physical_device() {
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);

	if (deviceCount == 0) {
		throw std::runtime_error("VULKAN ERROR: Failed to find a GPU with Vulkan support!");
	}

	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices.data());

	for (const auto& device : devices) {
		if (is_device_suitable(device)) {
			m_PhysicalDevice = device;
			break;
		}
	}

	if (m_PhysicalDevice == nullptr) {
		throw std::runtime_error("VULKAN ERROR: Failed to find a suitable GPU!");
	}
}


void GFXDevice_Vulkan::Impl::create_device() {
	const QueueFamilyIndices indices = find_queue_families(m_PhysicalDevice);

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos = {};
	const std::set<uint32_t> uniqueQueueFamilies = {
		indices.graphicsFamily.value(),
		indices.presentFamily.value()
	};

	const float queuePriority = 1.0f;
	for (uint32_t q : uniqueQueueFamilies) {
		const VkDeviceQueueCreateInfo queueCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.queueFamilyIndex = q,
			.queueCount = 1,
			.pQueuePriorities = &queuePriority
		};

		queueCreateInfos.push_back(queueCreateInfo);
	}

	// ------ Device features ------
	VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexingFeatures = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES,
		.pNext = nullptr
	};;

	VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
		.pNext = &descriptorIndexingFeatures,
		.dynamicRendering = VK_TRUE
	};

	VkPhysicalDeviceSynchronization2Features synchronizationFeatures = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES,
		.pNext = &dynamicRenderingFeatures,
		.synchronization2 = VK_TRUE,
	};

	VkPhysicalDeviceFeatures2 deviceFeatures = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
		.pNext = &synchronizationFeatures,
		.features = { .samplerAnisotropy = VK_TRUE }
	};

	vkGetPhysicalDeviceFeatures2(m_PhysicalDevice, &deviceFeatures);

	// Assert that certain descriptor indexing features are supported
	assert(descriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing);
	assert(descriptorIndexingFeatures.descriptorBindingSampledImageUpdateAfterBind);
	assert(descriptorIndexingFeatures.shaderUniformBufferArrayNonUniformIndexing);
	assert(descriptorIndexingFeatures.descriptorBindingUniformBufferUpdateAfterBind);
	assert(descriptorIndexingFeatures.shaderStorageBufferArrayNonUniformIndexing);
	assert(descriptorIndexingFeatures.descriptorBindingStorageBufferUpdateAfterBind);

	VkDeviceCreateInfo createInfo = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.pNext = &deviceFeatures,
		.flags = 0,
		.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
		.pQueueCreateInfos = queueCreateInfos.data(),
		.enabledExtensionCount = static_cast<uint32_t>(m_DeviceExtensions.size()),
		.ppEnabledExtensionNames = m_DeviceExtensions.data(),
	};

	if constexpr (m_EnableValidationLayers) {
		createInfo.enabledLayerCount = static_cast<uint32_t>(m_ValidationLayers.size());
		createInfo.ppEnabledLayerNames = m_ValidationLayers.data();
	}
	else {
		createInfo.enabledLayerCount = 0;
		createInfo.ppEnabledLayerNames = nullptr;
	}

	if (vkCreateDevice(m_PhysicalDevice, &createInfo, nullptr, &m_Device) != VK_SUCCESS) {
		throw std::runtime_error("VULKAN ERROR: Failed to create logical device!");
	}

	// TODO: Perhaps move this elsewhere?
	vkGetDeviceQueue(m_Device, indices.graphicsFamily.value(), 0, &m_GraphicsQueue);
	vkGetDeviceQueue(m_Device, indices.presentFamily.value(), 0, &m_PresentQueue);
}

void GFXDevice_Vulkan::Impl::create_destruction_handler() {
	m_DestructionHandler.device = m_Device;
	m_DestructionHandler.instance = m_Instance;
}

void GFXDevice_Vulkan::Impl::create_command_pool() {
	const QueueFamilyIndices queueFamilyIndices = find_queue_families(m_PhysicalDevice);

	const VkCommandPoolCreateInfo poolInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.pNext = nullptr,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value()
	};

	if (vkCreateCommandPool(m_Device, &poolInfo, nullptr, &m_CommandPool) != VK_SUCCESS) {
		throw std::runtime_error("VULKAN ERROR: Failed to create command pool!");
	}
}

void GFXDevice_Vulkan::Impl::create_sync_objects() {
	m_ImageAvailableSemaphores.resize(FRAMES_IN_FLIGHT);
	m_RenderFinishedSemaphores.resize(FRAMES_IN_FLIGHT);
	m_InFlightFences.resize(FRAMES_IN_FLIGHT);

	const VkSemaphoreCreateInfo semaphoreInfo = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
	};

	const VkFenceCreateInfo fenceInfo = {
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO
	};

	for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
		if (vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &m_ImageAvailableSemaphores[i]) != VK_SUCCESS ||
			vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &m_RenderFinishedSemaphores[i]) != VK_SUCCESS ||
			vkCreateFence(m_Device, &fenceInfo, nullptr, &m_InFlightFences[i]) != VK_SUCCESS) {

			throw std::runtime_error("VULKAN ERROR: Failed to create semaphores.");
		}
	}
}

void GFXDevice_Vulkan::Impl::create_descriptors() {
	// Descriptor pool
	const std::array<VkDescriptorPoolSize, 2> poolSizes = {
		VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_UBO_DESCRIPTORS },
		VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, MAX_TEXTURE_DESCRIPTORS }
	};

	const VkDescriptorPoolCreateInfo poolInfo = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
		.maxSets = MAX_UBO_DESCRIPTORS,
		.poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
		.pPoolSizes = poolSizes.data()
	};

	if (vkCreateDescriptorPool(m_Device, &poolInfo, nullptr, &m_DescriptorPool) != VK_SUCCESS) {
		throw std::runtime_error("VULKAN ERROR: Failed to create descriptor pool!");
	}

	// Bindless descriptor sets
	const std::array<VkDescriptorBindingFlags, 2> descriptorBindingFlags = {
		VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
		VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
	};
	const std::array<VkDescriptorType, 2> descriptorTypes = {
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE
	};
	const std::array<uint32_t, 2> descriptorCounts = {
		MAX_UBO_DESCRIPTORS,
		MAX_TEXTURE_DESCRIPTORS
	};
	std::array<VkDescriptorSetLayoutBinding, 2> descriptorBindings = {};

	for (uint32_t i = 0; i < 2; ++i) {
		descriptorBindings[i].binding = i;
		descriptorBindings[i].descriptorType = descriptorTypes[i];
		descriptorBindings[i].descriptorCount = descriptorCounts[i];
		descriptorBindings[i].stageFlags = VK_SHADER_STAGE_ALL;
		descriptorBindings[i].pImmutableSamplers = nullptr;
	}

	const VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
		.bindingCount = static_cast<uint32_t>(descriptorBindingFlags.size()),
		.pBindingFlags = descriptorBindingFlags.data()
	};

	const VkDescriptorSetLayoutCreateInfo setLayoutInfo = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.pNext = &bindingFlagsInfo,
		.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
		.bindingCount = static_cast<uint32_t>(descriptorBindings.size()),
		.pBindings = descriptorBindings.data()
	};

	if (vkCreateDescriptorSetLayout(m_Device, &setLayoutInfo, nullptr, &m_ResourceDescriptorSetLayout) != VK_SUCCESS) {
		throw std::runtime_error("VULKAN ERROR: Failed to create descriptor set layout!");
	}

	const VkDescriptorSetAllocateInfo descriptorSetAllocInfo = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = m_DescriptorPool,
		.descriptorSetCount = 1,
		.pSetLayouts = &m_ResourceDescriptorSetLayout
	};

	if (vkAllocateDescriptorSets(m_Device, &descriptorSetAllocInfo, &m_ResourceDescriptorSet) != VK_SUCCESS) {
		throw std::runtime_error("VULKAN ERROR: Failed to allocate descriptor sets!");
	}
}

bool GFXDevice_Vulkan::Impl::check_validation_layers() {
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	for (const char* layerName : m_ValidationLayers) {
		bool layerFound = false;

		for (const auto& layerProperties : availableLayers) {
			if (strcmp(layerName, layerProperties.layerName) == 0) {
				layerFound = true;
				break;
			}
		}

		if (!layerFound) {
			return false;
		}
	}

	return true;
}

bool GFXDevice_Vulkan::Impl::is_device_suitable(VkPhysicalDevice device) {
	VkPhysicalDeviceProperties deviceProperties;
	VkPhysicalDeviceFeatures deviceFeatures;
	vkGetPhysicalDeviceProperties(device, &deviceProperties);
	vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

	QueueFamilyIndices indices = find_queue_families(device);
	bool extensionsSupported = check_device_extension_support(device);

	bool swapChainAdequate = false;
	if (extensionsSupported) {
		SwapChainSupportInfo swapChainSupport = query_swapchain_support(device);
		swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
	}

	return indices.is_complete() && extensionsSupported && swapChainAdequate;
}

std::vector<const char*> GFXDevice_Vulkan::Impl::get_required_extensions() {
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

	if constexpr (m_EnableValidationLayers) {
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	return extensions;
}

VKAPI_ATTR VkBool32 VKAPI_CALL GFXDevice_Vulkan::Impl::debug_callback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData)
{
	std::cerr << "Validation layer: " << pCallbackData->pMessage << std::endl;
	return VK_FALSE;
}

VkResult GFXDevice_Vulkan::Impl::create_debug_utils_messenger(
	VkInstance instance,
	const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
	const VkAllocationCallbacks* pAllocator,
	VkDebugUtilsMessengerEXT* pDebugMessenger)
{
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr) {
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	}
	else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void GFXDevice_Vulkan::Impl::destroy_debug_utils_messenger(
	VkInstance instance,
	VkDebugUtilsMessengerEXT debugMessenger,
	const VkAllocationCallbacks* pAllocator)
{
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr) {
		func(instance, debugMessenger, pAllocator);
	}
}

void GFXDevice_Vulkan::Impl::populate_debug_messenger_create_info(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
	createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	createInfo.pfnUserCallback = debug_callback;
}

void GFXDevice_Vulkan::Impl::create_internal_swapchain(SwapChain_Vulkan* internalState) {
	VkSurfaceFormatKHR surfaceFormat{};
	surfaceFormat.format = to_vk_format(internalState->info.format);
	surfaceFormat.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

	SwapChainSupportInfo supportInfo = query_swapchain_support(m_PhysicalDevice);

	// TODO: Choose swap surface format in a more robust manner
	//for (const auto& format : supportInfo.formats) {
	//	if (format.format == VK_FORMAT_R8G8B8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
	//		surfaceFormat = format;
	//		break;
	//	}
	//}

	VkExtent2D extent = {};

	if (supportInfo.capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
		extent = supportInfo.capabilities.currentExtent;
	}
	else {
		int width;
		int height;
		glfwGetFramebufferSize(m_Window, &width, &height);

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

	internalState->extent = extent;

	uint32_t imageCount = supportInfo.capabilities.minImageCount;

	// Note: maxImageCount = 0 means that there is no maximum
	if (supportInfo.capabilities.maxImageCount > 0 && imageCount > supportInfo.capabilities.maxImageCount) {
		imageCount = supportInfo.capabilities.maxImageCount;
	}

	auto* oldSwapchain = internalState->swapChain;

	VkSwapchainCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = m_Surface;
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	createInfo.preTransform = supportInfo.capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR; // V-Sync: always guaranteed to be supported

	// If V-Sync is not desired, choose the best non-V-sync present mode
	if (!internalState->info.vsync) {
		for (const auto& mode : supportInfo.presentModes) {
			if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
				createInfo.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
				break;
			}
		}
	}

	const QueueFamilyIndices indices = find_queue_families(m_PhysicalDevice);
	const uint32_t queueFamilyIndices[] = {
		indices.graphicsFamily.value(),
		indices.presentFamily.value()
	};

	if (indices.graphicsFamily != indices.presentFamily) {
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else {
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.queueFamilyIndexCount = 0;
		createInfo.pQueueFamilyIndices = nullptr;
	}

	createInfo.clipped = VK_TRUE;
	createInfo.oldSwapchain = oldSwapchain;

	// HACK: It turns out that swapchain recreation is an underspecified portion of the
	// Vulkan spec at the moment, and the only way to "correctly" do it is to wait idle
	// before creating a new one. Another note is that acquisition of the swapchain
	// image actually is a call that does nothing because of some vendor stuff,
	// so it's not enough to rely on the swapchain being invalidated.
	if (oldSwapchain != VK_NULL_HANDLE) {
		//waitForGPU();
	}

	if (vkCreateSwapchainKHR(m_Device, &createInfo, nullptr, &internalState->swapChain) != VK_SUCCESS) {
		throw std::runtime_error("VULKAN ERROR: Failed to create swap chain.");
	}

	if (oldSwapchain != VK_NULL_HANDLE) {
		m_DestructionHandler.swapchains.push_back({ oldSwapchain, m_DestructionHandler.frameCount });
	}

	// Swapchain images
	vkGetSwapchainImagesKHR(m_Device, internalState->swapChain, &imageCount, nullptr);
	internalState->images.resize(imageCount);
	vkGetSwapchainImagesKHR(m_Device, internalState->swapChain, &imageCount, internalState->images.data());

	// Create swapchain image views
	internalState->imageViews.resize(internalState->images.size());

	for (size_t i = 0; i < internalState->images.size(); i++) {
		VkImageViewCreateInfo imageViewInfo{};
		imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewInfo.image = internalState->images[i];
		imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewInfo.format = surfaceFormat.format;
		imageViewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageViewInfo.subresourceRange.baseMipLevel = 0;
		imageViewInfo.subresourceRange.levelCount = 1;
		imageViewInfo.subresourceRange.baseArrayLayer = 0;
		imageViewInfo.subresourceRange.layerCount = 1;

		if (internalState->imageViews[i] != VK_NULL_HANDLE) {
			//m_DestructionHandler->imageViews.push_back({ internalState->imageViews[i], m_DestructionHandler->frameCount });
		}

		if (vkCreateImageView(m_Device, &imageViewInfo, nullptr, &internalState->imageViews[i]) != VK_SUCCESS) {
			throw std::runtime_error("VULKAN ERROR: Failed to create swapchain image views.");
		}
	}
}

GFXDevice_Vulkan::Impl::QueueFamilyIndices GFXDevice_Vulkan::Impl::find_queue_families(VkPhysicalDevice device) {
	QueueFamilyIndices indices = {};

	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

	int i = 0;
	for (const auto& queueFamily : queueFamilies) {
		if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			indices.graphicsFamily = i;
		}

		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_Surface, &presentSupport);

		if (presentSupport) {
			indices.presentFamily = i;
		}

		if (indices.is_complete()) {
			break;
		}

		i++;
	}

	return indices;
}

uint32_t GFXDevice_Vulkan::Impl::find_memory_type(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(m_PhysicalDevice, &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}

	throw std::runtime_error("VULKAN ERROR: Failed to find suitable memory type!");
}

bool GFXDevice_Vulkan::Impl::check_device_extension_support(VkPhysicalDevice device) {
	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

	std::set<std::string> requiredExtensions(m_DeviceExtensions.begin(), m_DeviceExtensions.end());

	for (const auto& extension : availableExtensions) {
		requiredExtensions.erase(extension.extensionName);
	}

	return requiredExtensions.empty();
}

GFXDevice_Vulkan::Impl::SwapChainSupportInfo GFXDevice_Vulkan::Impl::query_swapchain_support(VkPhysicalDevice device) {
	SwapChainSupportInfo info = {};

	// Capabilities
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_Surface, &info.capabilities);

	// Formats
	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_Surface, &formatCount, nullptr);

	if (formatCount != 0) {
		info.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_Surface, &formatCount, info.formats.data());
	}

	// Present modes
	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_Surface, &presentModeCount, nullptr);

	if (presentModeCount != 0) {
		info.presentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_Surface, &presentModeCount, info.presentModes.data());
	}

	return info;
}

// GFXDevice_Vulkan Interface
GFXDevice_Vulkan::GFXDevice_Vulkan(GLFWwindow* window) : GFXDevice(window) {
	m_Impl = new Impl(window);
}

GFXDevice_Vulkan::~GFXDevice_Vulkan() {
	delete m_Impl;
}

void GFXDevice_Vulkan::create_swapchain(const SwapChainInfo& info, SwapChain& swapChain) {
	if (swapChain.internalState != nullptr) { // recreation of swapchain detected
		m_Impl->create_internal_swapchain((SwapChain_Vulkan*)swapChain.internalState.get());
		return;
	}

	auto internalState = std::make_shared<SwapChain_Vulkan>();
	internalState->info = info;
	internalState->destructionHandler = &m_Impl->m_DestructionHandler;

	swapChain.info = info;
	swapChain.internalState = internalState;

	m_Impl->create_internal_swapchain(internalState.get());
}

void GFXDevice_Vulkan::create_pipeline(const PipelineInfo& info, Pipeline& pipeline) {
	auto internalState = std::make_shared<Pipeline_Vulkan>();
	internalState->info = info;
	internalState->destructionHandler = &m_Impl->m_DestructionHandler;

	pipeline.info = info;
	pipeline.internalState = internalState;

	std::vector<VkPipelineShaderStageCreateInfo> shaderStages = {};

	// Vertex shader
	if (info.vertexShader != nullptr) {
		auto internalShader = to_internal(*info.vertexShader);

		const VkShaderModuleCreateInfo shaderModuleInfo = {
			.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
			.codeSize = internalShader->shaderCode.size(),
			.pCode = reinterpret_cast<uint32_t*>(internalShader->shaderCode.data())
		};

		if (vkCreateShaderModule(
			m_Impl->m_Device,
			&shaderModuleInfo,
			nullptr,
			&internalShader->shaderModule) != VK_SUCCESS) {
			throw std::runtime_error("VULKAN ERROR: Failed to create vertex shader module!");
		}

		const VkPipelineShaderStageCreateInfo shaderStageInfo = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = internalShader->shaderModule,
			.pName = "main"
		};

		shaderStages.push_back(shaderStageInfo);
	}

	// Pixel shader
	if (info.pixelShader != nullptr) {
		auto internalShader = to_internal(*info.pixelShader);

		const VkShaderModuleCreateInfo shaderModuleInfo = {
			.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
			.codeSize = internalShader->shaderCode.size(),
			.pCode = reinterpret_cast<uint32_t*>(internalShader->shaderCode.data())
		};

		if (vkCreateShaderModule(
			m_Impl->m_Device,
			&shaderModuleInfo,
			nullptr,
			&internalShader->shaderModule) != VK_SUCCESS) {
			throw std::runtime_error("VULKAN ERROR: Failed to create pixel shader module!");
		}

		const VkPipelineShaderStageCreateInfo shaderStageInfo = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = internalShader->shaderModule,
			.pName = "main"
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
		attributeDescriptions[i].location = i; // TODO: Doesn't work for all formats
		attributeDescriptions[i].format = to_vk_format(info.inputLayout.elements[i].format);
		attributeDescriptions[i].offset = offset;

		offset += get_format_stride(info.inputLayout.elements[i].format);
	}

	// TODO: For now we only allow one binding description
	const VkVertexInputBindingDescription bindingDescription = {
		.binding = 0,
		.stride = offset, // total offset is equivalent to stride
		.inputRate = VK_VERTEX_INPUT_RATE_VERTEX
	};

	const VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions = &bindingDescription,
		.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size()),
		.pVertexAttributeDescriptions = attributeDescriptions.data()
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
		.cullMode = VK_CULL_MODE_NONE,
		.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
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

	const VkPipelineColorBlendAttachmentState colorBlendAttachment = {
		.blendEnable = VK_FALSE,
		.srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
		.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
		.colorBlendOp = VK_BLEND_OP_ADD,
		.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
		.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
		.alphaBlendOp = VK_BLEND_OP_ADD,
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
	};

	const VkPipelineColorBlendStateCreateInfo colorBlendInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.logicOpEnable = VK_FALSE,
		.logicOp = VK_LOGIC_OP_COPY,
		.attachmentCount = 1,
		.pAttachments = &colorBlendAttachment,
		.blendConstants = { 0.0f, 0.0f, 0.0f, 0.0f }
	};

	// Bindless descriptors
	const VkDescriptorSetLayout descriptorSetLayouts[1] = {
		m_Impl->m_ResourceDescriptorSetLayout
	};

	const VkPushConstantRange pushConstantRange = {
		.stageFlags = VK_SHADER_STAGE_ALL,
		.offset = 0,
		.size = 128
	};

	const VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = descriptorSetLayouts,
		.pushConstantRangeCount = 1,
		.pPushConstantRanges = &pushConstantRange
	};

	if (vkCreatePipelineLayout(m_Impl->m_Device, &pipelineLayoutInfo, nullptr, &internalState->pipelineLayout) != VK_SUCCESS) {
		throw std::runtime_error("VULKAN ERROR: Failed to create pipeline layout!");
	}

	std::vector<VkFormat> colorAttachmentFormats = {};
	colorAttachmentFormats.reserve(static_cast<size_t>(info.numRenderTargets));

	for (size_t i = 0; i < info.numRenderTargets; ++i) {
		colorAttachmentFormats.push_back(to_vk_format(info.renderTargetFormats[i]));
	}

	const VkPipelineRenderingCreateInfo pipelineRenderingInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		.colorAttachmentCount = info.numRenderTargets,
		.pColorAttachmentFormats = colorAttachmentFormats.data(),
		.depthAttachmentFormat = to_vk_format(info.depthStencilFormat)
	};

	VkPipelineDepthStencilStateCreateInfo depthStencilInfo{};
	depthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

	// TODO: Enable depth


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
		.layout = internalState->pipelineLayout,
		.renderPass = nullptr,
		.subpass = 0,
		.basePipelineHandle = VK_NULL_HANDLE,
		.basePipelineIndex = -1
	};

	if (vkCreateGraphicsPipelines(m_Impl->m_Device, nullptr, 1, &pipelineInfo, nullptr, &internalState->pipeline) != VK_SUCCESS) {
		throw std::runtime_error("VULKAN ERROR: Failed to create graphics pipeline!");
	}
}

void GFXDevice_Vulkan::create_buffer(const BufferInfo& info, Buffer& buffer, const void* data) {
	auto internalState = std::make_shared<Impl::Buffer_Vulkan>();
	internalState->info = info;
	internalState->destructionHandler = &m_Impl->m_DestructionHandler;

	buffer.info = info;
	buffer.type = Resource::Type::BUFFER;
	buffer.mappedData = nullptr;
	buffer.mappedSize = 0;
	buffer.internalState = internalState;

	VkBufferCreateInfo bufferInfo = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = info.size,
		.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, // NOTE: This has no performance cost, and is handy for staging buffer creation
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	if (has_flag(info.bindFlags, BindFlag::VERTEX_BUFFER)) {
		bufferInfo.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	}
	else if (has_flag(info.bindFlags, BindFlag::INDEX_BUFFER)) {
		bufferInfo.usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	}
	else if (has_flag(info.bindFlags, BindFlag::UNIFORM_BUFFER)) {
		bufferInfo.usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	}

	if (vkCreateBuffer(m_Impl->m_Device, &bufferInfo, nullptr, &internalState->buffer) != VK_SUCCESS) {
		throw std::runtime_error("VULKAN ERROR: Failed to create buffer!");
	}

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(m_Impl->m_Device, internalState->buffer, &memRequirements);

	VkMemoryPropertyFlags memPropertyFlags = 0;

	switch (info.usage) {
	case Usage::DEFAULT:
		memPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		break;
	case Usage::UPLOAD:
		memPropertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT; // coherent flag might not be needed
		break;
	}

	const uint32_t memoryType = m_Impl->find_memory_type(memRequirements.memoryTypeBits, memPropertyFlags);
	const VkMemoryAllocateInfo allocInfo = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = memRequirements.size,
		.memoryTypeIndex = memoryType
	};

	if (vkAllocateMemory(m_Impl->m_Device, &allocInfo, nullptr, &internalState->bufferMemory) != VK_SUCCESS) {
		throw std::runtime_error("VULKAN ERROR: Failed to allocate buffer memory!");
	}

	vkBindBufferMemory(m_Impl->m_Device, internalState->buffer, internalState->bufferMemory, 0);

	if (info.usage == Usage::DEFAULT) {
		// Create staging buffer
		const BufferInfo stagingBufferInfo{
			.size = info.size,
			.stride = info.stride,
			.usage = Usage::UPLOAD,
			.bindFlags = BindFlag::NONE, // TODO: Might break for shader resources
		};

		Buffer stagingBuffer = {};
		create_buffer(stagingBufferInfo, stagingBuffer, data);
		auto internalStagingBuffer = m_Impl->to_internal(stagingBuffer);

		// Temporary command list for copy
		const VkCommandBufferAllocateInfo tempAllocInfo = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool = m_Impl->m_CommandPool,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = 1
		};

		VkCommandBuffer tempCommandBuffer;
		vkAllocateCommandBuffers(m_Impl->m_Device, &tempAllocInfo, &tempCommandBuffer);

		const VkCommandBufferBeginInfo tempBeginInfo = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
		};

		vkBeginCommandBuffer(tempCommandBuffer, &tempBeginInfo);
		{
			const VkBufferCopy copyRegion = {
				.srcOffset = 0,
				.dstOffset = 0,
				.size = info.size
			};

			vkCmdCopyBuffer(
				tempCommandBuffer,
				internalStagingBuffer->buffer,
				internalState->buffer,
				1,
				&copyRegion
			);
		}
		vkEndCommandBuffer(tempCommandBuffer);

		const VkSubmitInfo submitInfo = {
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.commandBufferCount = 1,
			.pCommandBuffers = &tempCommandBuffer
		};

		vkQueueSubmit(m_Impl->m_GraphicsQueue, 1, &submitInfo, nullptr);
		vkQueueWaitIdle(m_Impl->m_GraphicsQueue);
		vkFreeCommandBuffers(m_Impl->m_Device, m_Impl->m_CommandPool, 1, &tempCommandBuffer);
	}
	else if (info.usage == Usage::UPLOAD) {
		vkMapMemory(m_Impl->m_Device, internalState->bufferMemory, 0, info.size, 0, &buffer.mappedData);

		if (data != nullptr) {
			std::memcpy(buffer.mappedData, data, info.size);
		}

		if (!info.persistentMap) {
			vkUnmapMemory(m_Impl->m_Device, internalState->bufferMemory);
		}
	}

	// Descriptors
	if (has_flag(info.bindFlags, BindFlag::UNIFORM_BUFFER)) {
		internalState->descriptor.init_ubo(m_Impl, internalState->buffer);
	}
}

void GFXDevice_Vulkan::create_shader(ShaderStage stage, const std::string& path, Shader& shader) {
	auto internalState = std::make_shared<Shader_Vulkan>();
	internalState->destructionHandler = &m_Impl->m_DestructionHandler;

	shader.internalState = internalState;

	const std::string fullPath = ENGINE_BASE_DIR + path;
	std::ifstream file(fullPath, std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
		throw std::runtime_error("VULKAN ERROR: Failed to open SPIRV shader file.");
	}

	const size_t fileSize = static_cast<size_t>(file.tellg());
	internalState->shaderCode.resize(fileSize);

	file.seekg(0);
	file.read(reinterpret_cast<char*>(internalState->shaderCode.data()), fileSize);
	file.close();
}

void GFXDevice_Vulkan::create_texture(const TextureInfo& info, Texture& texture, const SubresourceData* data) {

}

void GFXDevice_Vulkan::bind_pipeline(const Pipeline& pipeline, const CommandList& cmdList) {
	auto internalPipeline = to_internal(pipeline);
	auto internalCmdList = to_internal(cmdList);

	m_Impl->m_ActivePipeline = internalPipeline;

	VkCommandBuffer currVkCommandBuffer = internalCmdList->commandBuffers[m_CurrentFrame];

	vkCmdBindPipeline(
		currVkCommandBuffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		internalPipeline->pipeline
	);

	vkCmdBindDescriptorSets(
		currVkCommandBuffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		internalPipeline->pipelineLayout,
		0,
		1,
		&m_Impl->m_ResourceDescriptorSet,
		0,
		nullptr
	);
}

void GFXDevice_Vulkan::bind_viewport(const Viewport& viewport, const CommandList& cmdList) {
	auto internalCmdList = to_internal(cmdList);

	// We need to flip the viewport vertically in order to interoperate
	// with DX12 and Metal backends.
	const VkViewport vkViewport = {
		.x = viewport.topLeftX,
		.y = viewport.topLeftY + viewport.height,
		.width = viewport.width,
		.height = -viewport.height,
		.minDepth = viewport.minDepth,
		.maxDepth = viewport.maxDepth
	};

	// TODO: Move scissor rect
	const VkExtent2D scissorExtent = {
		.width = static_cast<uint32_t>(viewport.width),
		.height = static_cast<uint32_t>(viewport.height)
	};

	const VkRect2D scissor = {
		.offset = { 0, 0 },
		.extent = scissorExtent
	};

	vkCmdSetViewport(internalCmdList->commandBuffers[m_CurrentFrame], 0, 1, &vkViewport);
	vkCmdSetScissor(internalCmdList->commandBuffers[m_CurrentFrame], 0, 1, &scissor);
}

void GFXDevice_Vulkan::bind_uniform_buffer(const Buffer& uniformBuffer, uint32_t slot) {

}

void GFXDevice_Vulkan::bind_vertex_buffer(const Buffer& vertexBuffer, const CommandList& cmdList) {
	auto internalVertexBuffer = m_Impl->to_internal(vertexBuffer);
	auto internalCmdList = to_internal(cmdList);
	VkCommandBuffer currVkCommandBuffer = internalCmdList->commandBuffers[m_CurrentFrame];

	const VkBuffer vertexBuffers[] = { internalVertexBuffer->buffer };
	const VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(currVkCommandBuffer, 0, 1, vertexBuffers, offsets);
}

void GFXDevice_Vulkan::bind_index_buffer(const Buffer& indexBuffer, const CommandList& cmdList) {
	auto internalIndexBuffer = m_Impl->to_internal(indexBuffer);
	auto internalCmdList = to_internal(cmdList);
	VkCommandBuffer currVkCommandBuffer = internalCmdList->commandBuffers[m_CurrentFrame];

	vkCmdBindIndexBuffer(currVkCommandBuffer, internalIndexBuffer->buffer, 0, VK_INDEX_TYPE_UINT32);
}

void GFXDevice_Vulkan::bind_resource(const Resource& resource, uint32_t slot) {

}

void GFXDevice_Vulkan::push_constants(const void* data, uint32_t size, const CommandList& cmdList) {
	assert(data != nullptr);
	assert(size <= 128); 
	assert(m_Impl->m_ActivePipeline != nullptr);

	auto internalCmdList = to_internal(cmdList);

	vkCmdPushConstants(
		internalCmdList->commandBuffers[m_CurrentFrame],
		m_Impl->m_ActivePipeline->pipelineLayout,
		VK_SHADER_STAGE_ALL,
		0,
		size,
		data
	);
}

CommandList GFXDevice_Vulkan::begin_command_list(QueueType queue) {
	const size_t currCmdListIndex = m_Impl->m_CmdListCounter++;

	if (currCmdListIndex >= m_Impl->m_CmdLists.size()) {
		m_Impl->m_CmdLists.push_back(std::make_unique<CommandList_Vulkan>());
	}

	CommandList cmdList = {};
	cmdList.internalState = m_Impl->m_CmdLists[currCmdListIndex].get();

	auto internalCmdList = to_internal(cmdList);

	if (internalCmdList->commandBuffers[0] == nullptr) {
		// Create one command buffer per frame in flight
		for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
			const VkCommandBufferAllocateInfo allocInfo = {
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
				.commandPool = m_Impl->m_CommandPool,
				.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
				.commandBufferCount = 1
			};

			if (vkAllocateCommandBuffers(m_Impl->m_Device, &allocInfo, &internalCmdList->commandBuffers[i]) != VK_SUCCESS) {
				throw std::runtime_error("VULKAN ERROR: Failed to allocate command buffer!");
			}
		}
	}

	if (vkResetCommandPool(m_Impl->m_Device, m_Impl->m_CommandPool, 0) != VK_SUCCESS) {
		throw std::runtime_error("VULKAN ERROR: Failed to reset command pool!");
	}

	// Begin command buffer recording
	const VkCommandBufferBeginInfo beginInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = 0,
		.pInheritanceInfo = nullptr
	};

	if (vkBeginCommandBuffer(internalCmdList->commandBuffers[m_CurrentFrame], &beginInfo) != VK_SUCCESS) {
		throw std::runtime_error("VULKAN ERROR: Failed to begin recording of command buffer.");
	}

	return cmdList;
}

void GFXDevice_Vulkan::begin_render_pass(const SwapChain& swapChain, const PassInfo& passInfo, const CommandList& cmdList) {
	auto internalSwapChain = to_internal(swapChain);
	auto internalCmdList = to_internal(cmdList);

	const VkResult res = vkAcquireNextImageKHR(
		m_Impl->m_Device,
		internalSwapChain->swapChain,
		std::numeric_limits<uint64_t>::max(),
		m_Impl->m_ImageAvailableSemaphores[m_CurrentFrame],
		nullptr,
		&m_CurrentImageIndex
	);

	if (res != VK_SUCCESS) {
		throw std::runtime_error("VULKAN ERROR: Failed to acquire next image!");
	}

	VkCommandBuffer currVkCommandBuffer = internalCmdList->commandBuffers[m_CurrentFrame];

	const vk_helpers::ImageTransitionInfo transitionInfo = {
		.image = internalSwapChain->images[m_CurrentImageIndex],
		.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.srcAccessMask = VK_ACCESS_2_NONE,
		.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
		.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT
	};
	vk_helpers::transition_image_layout(transitionInfo, currVkCommandBuffer);

	const VkClearValue clearColor = { { { 0.0f, 0.0f, 0.0f, 1.0f } } };

	const VkRenderingAttachmentInfo colorAttachmentInfo = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
		.imageView = internalSwapChain->imageViews[m_CurrentImageIndex],
		.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.clearValue = clearColor
	};

	// TODO: Depth
	// ...

	const VkRect2D area = {
		.offset = { 0, 0 },
		.extent = internalSwapChain->extent
	};

	const VkRenderingInfo renderInfo = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
		.renderArea = area,
		.layerCount = 1,
		.colorAttachmentCount = 1,
		.pColorAttachments = &colorAttachmentInfo
	};

	// TODO: Depth attachment
	// ...

	vkCmdBeginRendering(currVkCommandBuffer, &renderInfo);
}

void GFXDevice_Vulkan::end_render_pass(const SwapChain& swapChain, const CommandList& cmdList) {
	auto internalSwapChain = to_internal(swapChain);
	auto internalCmdList = to_internal(cmdList);

	VkCommandBuffer currVkCommandBuffer = internalCmdList->commandBuffers[m_CurrentFrame];
	vkCmdEndRendering(currVkCommandBuffer);

	const vk_helpers::ImageTransitionInfo transitionInfo = {
		.image = internalSwapChain->images[m_CurrentImageIndex],
		.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
		.dstAccessMask = VK_ACCESS_2_NONE,
		.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT
	};
	vk_helpers::transition_image_layout(transitionInfo, currVkCommandBuffer);
}

void GFXDevice_Vulkan::submit_command_lists(const SwapChain& swapChain) {
	auto internalSwapChain = to_internal(swapChain);

	const uint32_t tempCmdListCount = m_Impl->m_CmdListCounter;
	m_Impl->m_CmdListCounter = 0;

	for (uint32_t i = 0; i < tempCmdListCount; i++) {
		const CommandList_Vulkan* cmdList = m_Impl->m_CmdLists[i].get();

		if (vkEndCommandBuffer(cmdList->commandBuffers[m_CurrentFrame]) != VK_SUCCESS) {
			throw std::runtime_error("VULKAN ERROR: Failed to finish recording of command buffer!");
		}

		const VkSemaphore waitSemaphores[] = { m_Impl->m_ImageAvailableSemaphores[m_CurrentFrame] };
		const VkSemaphore signalSemaphores[] = { m_Impl->m_RenderFinishedSemaphores[m_CurrentFrame] };
		const VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

		const VkSubmitInfo submitInfo = {
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = waitSemaphores,
			.pWaitDstStageMask = waitStages,
			.commandBufferCount = 1,
			.pCommandBuffers = &cmdList->commandBuffers[m_CurrentFrame],
			.signalSemaphoreCount = 1,
			.pSignalSemaphores = signalSemaphores
		};

		if (vkQueueSubmit(m_Impl->m_GraphicsQueue, 1, &submitInfo, m_Impl->m_InFlightFences[m_CurrentFrame]) != VK_SUCCESS) {
			throw std::runtime_error("VULKAN ERROR: Failed to submit draw command buffer!");
		}

		const VkPresentInfoKHR presentInfo = {
			.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = signalSemaphores,
			.swapchainCount = 1,
			.pSwapchains = &internalSwapChain->swapChain,
			.pImageIndices = &m_CurrentImageIndex
		};

		const VkResult res = vkQueuePresentKHR(m_Impl->m_PresentQueue, &presentInfo);

		if (res != VK_SUCCESS) {
			throw std::runtime_error("VULKAN ERROR: Failed to queue present KHR!");
		}
	}

	m_FrameCount++;

	vkWaitForFences(m_Impl->m_Device, 1, &m_Impl->m_InFlightFences[m_CurrentFrame], VK_TRUE, std::numeric_limits<uint64_t>::max());
	vkResetFences(m_Impl->m_Device, 1, &m_Impl->m_InFlightFences[m_CurrentFrame]);

	m_CurrentFrame = (m_CurrentFrame + 1) % FRAMES_IN_FLIGHT;

	m_Impl->m_DestructionHandler.update(m_FrameCount, FRAMES_IN_FLIGHT);
}

void GFXDevice_Vulkan::update_buffer(const Buffer& buffer, const void* data) {

}

void GFXDevice_Vulkan::draw(uint32_t vertexCount, uint32_t startVertex, const CommandList& cmdList) {
	auto internalCommandBuffer = to_internal(cmdList);

	vkCmdDraw(internalCommandBuffer->commandBuffers[m_CurrentFrame], vertexCount, 1, startVertex, 0);
}

void GFXDevice_Vulkan::draw_indexed(uint32_t indexCount, uint32_t startIndex, uint32_t baseVertex, const CommandList& cmdList) {
	auto internalCommandBuffer = to_internal(cmdList);

	vkCmdDrawIndexed(
		internalCommandBuffer->commandBuffers[m_CurrentFrame],
		indexCount,
		1,
		startIndex,
		static_cast<int32_t>(baseVertex),
		0
	);
}

void GFXDevice_Vulkan::wait_for_gpu() {
	vkDeviceWaitIdle(m_Impl->m_Device);
}
