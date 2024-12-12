#include "gfx_device_vulkan.h"

#include "gfx_types_vulkan.h"
#include "../../platform.h"
#include "../../math/sr_math.h"

#include <GLFW/glfw3.h>
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

// GFXDevice_Vulkan Implemenation
struct GFXDevice_Vulkan::Impl {
	Impl(GLFWwindow* window);
	~Impl();

	struct Descriptor {
		uint32_t index = 0;

		void init_ubo(Impl* impl, VkBuffer buffer);
		void init_texture(Impl* impl, VkImageView imageView, VkImageLayout layout);
		void init_storage_buffer(Impl* impl, VkBuffer buffer);
		void init_rw_texture(Impl* impl, VkImageView imageView, VkImageLayout layout);
	};

	// NOTE: In Vulkan, we do not need separate descriptor pools
	// like in DX12 where we need different descriptor heap types.
	// However, we use a "DescriptorHeap" structure here to allow
	// for simpler descriptor indexing lookup based on descriptor
	// type. I.e. some resource might use different "DescriptorHeap"
	// but they will all be allocated from the same VkDescriptorPool
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
		VkDeviceAddress address = {};
	};

	struct Texture_Vulkan {
		~Texture_Vulkan() {
			const uint64_t frameCount = destructionHandler->frameCount;
			destructionHandler->imageViews.push_back({ imageView, frameCount });
			destructionHandler->images.push_back({ image, frameCount });
			destructionHandler->allocations.push_back({ imageMemory, frameCount });
		}

		DestructionHandler* destructionHandler = nullptr;
		Descriptor descriptor = {};
		Descriptor uavDescriptor = {};
		VkImage image = nullptr;
		VkImageView imageView = nullptr;
		VkDeviceMemory imageMemory = nullptr;
	};

	Buffer_Vulkan* to_internal(const Buffer& buffer) {
		return (Buffer_Vulkan*)buffer.internalState.get();
	}

	Texture_Vulkan* to_internal(const Texture& texture) {
		return (Texture_Vulkan*)texture.internalState.get();
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

	// Ray Tracing
	VkPhysicalDeviceRayTracingPipelinePropertiesKHR m_RTProperties = {
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
	};

	// Descriptors
	static constexpr uint32_t UBO_BINDING = 0;
	static constexpr uint32_t TEXTURE_BINDING = 1;
	static constexpr uint32_t SAMPLER_BINDING = 2;
	static constexpr uint32_t STORAGE_BUFFER_BINDING = 3;
	static constexpr uint32_t RW_TEXTURE_BINDING = 4;
	static constexpr uint32_t TLAS_BINDING = 5;

	VkDescriptorPool m_DescriptorPool = nullptr;
	VkDescriptorSet m_ResourceDescriptorSet = nullptr;
	VkDescriptorSetLayout m_ResourceDescriptorSetLayout = nullptr;
	DescriptorHeap m_UBODescriptorHeap = DescriptorHeap(GFXDevice::MAX_UBO_DESCRIPTORS);
	DescriptorHeap m_TextureDescriptorHeap = DescriptorHeap(GFXDevice::MAX_TEXTURE_DESCRIPTORS);
	DescriptorHeap m_StorageBufferDescriptorHeap = DescriptorHeap(GFXDevice::MAX_STORAGE_BUFFERS);
	DescriptorHeap m_RWTextureDescriptorHeap = DescriptorHeap(GFXDevice::MAX_RW_TEXTURE_DESCRIPTORS);

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

		// Ray Tracing
		VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
		VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
		VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
	};

	#ifdef NDEBUG
		inline static bool m_EnableValidationLayers = false;
	#else
		inline static bool m_EnableValidationLayers = true;
	#endif
};

void GFXDevice_Vulkan::Impl::Descriptor::init_ubo(Impl* impl, VkBuffer buffer) {
	const VkDescriptorBufferInfo descriptorBufferInfo = {
		.buffer = buffer,
		.offset = 0,
		.range = VK_WHOLE_SIZE
	};

	// TODO: Perhaps accumulate descriptor writes instead of one at a time?
	index = impl->m_UBODescriptorHeap.getCurrentDescriptorHandle();
	impl->m_UBODescriptorHeap.offsetCurrentDescriptorHandle(1);

	const VkWriteDescriptorSet descriptorWrite = {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = impl->m_ResourceDescriptorSet,
		.dstBinding = Impl::UBO_BINDING, // TODO: Remove hardcoded value
		.dstArrayElement = index,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.pBufferInfo = &descriptorBufferInfo
	};

	vkUpdateDescriptorSets(impl->m_Device, 1, &descriptorWrite, 0, nullptr);
}

void GFXDevice_Vulkan::Impl::Descriptor::init_texture(Impl* impl, VkImageView imageView, VkImageLayout layout) {
	const VkDescriptorImageInfo descriptorImageInfo = {
		.sampler = nullptr,
		.imageView = imageView,
		.imageLayout = layout
	};

	index = impl->m_TextureDescriptorHeap.getCurrentDescriptorHandle();
	impl->m_TextureDescriptorHeap.offsetCurrentDescriptorHandle(1);

	const VkWriteDescriptorSet descriptorWrite = {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = impl->m_ResourceDescriptorSet,
		.dstBinding = TEXTURE_BINDING,
		.dstArrayElement = index,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
		.pImageInfo = &descriptorImageInfo
	};

	vkUpdateDescriptorSets(impl->m_Device, 1, &descriptorWrite, 0, nullptr);
}

void GFXDevice_Vulkan::Impl::Descriptor::init_rw_texture(Impl* impl, VkImageView imageView, VkImageLayout layout) {
	const VkDescriptorImageInfo descriptorImageInfo = {
		.sampler = nullptr,
		.imageView = imageView,
		.imageLayout = layout
	};

	index = impl->m_RWTextureDescriptorHeap.getCurrentDescriptorHandle();
	impl->m_RWTextureDescriptorHeap.offsetCurrentDescriptorHandle(1);

	const VkWriteDescriptorSet descriptorWrite = {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = impl->m_ResourceDescriptorSet,
		.dstBinding = RW_TEXTURE_BINDING,
		.dstArrayElement = index,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
		.pImageInfo = &descriptorImageInfo
	};

	vkUpdateDescriptorSets(impl->m_Device, 1, &descriptorWrite, 0, nullptr);
}

void GFXDevice_Vulkan::Impl::Descriptor::init_storage_buffer(Impl* impl, VkBuffer buffer) {
	const VkDescriptorBufferInfo descriptorBufferInfo = {
		.buffer = buffer,
		.offset = 0,
		.range = VK_WHOLE_SIZE
	};

	index = impl->m_StorageBufferDescriptorHeap.getCurrentDescriptorHandle();
	impl->m_StorageBufferDescriptorHeap.offsetCurrentDescriptorHandle(1);

	const VkWriteDescriptorSet descriptorWrite = {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = impl->m_ResourceDescriptorSet,
		.dstBinding = Impl::STORAGE_BUFFER_BINDING,
		.dstArrayElement = index,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
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
	if (m_EnableValidationLayers) {
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

	// Destroy descriptor objects
	m_DestructionHandler.descriptorSetLayouts.push_back({ m_ResourceDescriptorSetLayout, frameCount });
	m_DestructionHandler.descriptorPools.push_back({ m_DescriptorPool, frameCount });
}

void GFXDevice_Vulkan::Impl::create_instance() {
	if (volkInitialize() != VK_SUCCESS) {
		throw std::runtime_error("VOLK ERROR: Failed to initialize Volk!");
	}

	const VkApplicationInfo appInfo = {
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pNext = nullptr,
		.pApplicationName = "Stingray",
		.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),
		.pEngineName = "Stingray",
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
	if (m_EnableValidationLayers) {
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

	volkLoadInstanceOnly(m_Instance);

	if (m_EnableValidationLayers && !check_validation_layers()) {
		throw std::runtime_error("VULKAN ERROR: Validation layers not available!");
	}
}

void GFXDevice_Vulkan::Impl::create_debug_messenger() {
	if (!m_EnableValidationLayers) {
		return;
	}

	VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
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

	// ---------------------------- Device features ----------------------------
	VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationFeatures = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
		.pNext = nullptr
	};

	VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeatures = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
		.pNext = &accelerationFeatures
	};

	VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexingFeatures = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES,
		.pNext = &rtPipelineFeatures
	};

	VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
		.pNext = &descriptorIndexingFeatures,
		.bufferDeviceAddress = VK_TRUE
	};

	VkPhysicalDeviceScalarBlockLayoutFeatures scalarLayoutFeatures = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES,
		.pNext = &bufferDeviceAddressFeatures,
		.scalarBlockLayout = VK_TRUE
	};

	VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
		.pNext = &scalarLayoutFeatures,
		.dynamicRendering = VK_TRUE
	};

	VkPhysicalDeviceSynchronization2Features synchronizationFeatures = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES,
		.pNext = &dynamicRenderingFeatures,
		.synchronization2 = VK_TRUE
	};

	VkPhysicalDeviceFeatures2 deviceFeatures = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
		.pNext = &synchronizationFeatures,
		.features = { .samplerAnisotropy = VK_TRUE }
	};

	vkGetPhysicalDeviceFeatures2(m_PhysicalDevice, &deviceFeatures);

	// Assert that device features are supported
	assert(accelerationFeatures.accelerationStructure);
	assert(rtPipelineFeatures.rayTracingPipeline);

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

	if (m_EnableValidationLayers) {
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

	volkLoadDevice(m_Device);

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
	const std::array<VkDescriptorPoolSize, 6> poolSizes = {
		VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_UBO_DESCRIPTORS },
		VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, MAX_TEXTURE_DESCRIPTORS },
		VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_SAMPLER, MAX_SAMPLER_DESCRIPTORS },
		VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, MAX_STORAGE_BUFFERS },
		VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, MAX_RW_TEXTURE_DESCRIPTORS },
		VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, MAX_RAY_TRACING_TLASES } // TODO: Look into how we should best handle AS descriptors (realistically we will only ever have one TLAS descriptor)
	};

	const VkDescriptorPoolCreateInfo poolInfo = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
		.maxSets = 1,
		.poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
		.pPoolSizes = poolSizes.data()
	};

	if (vkCreateDescriptorPool(m_Device, &poolInfo, nullptr, &m_DescriptorPool) != VK_SUCCESS) {
		throw std::runtime_error("VULKAN ERROR: Failed to create descriptor pool!");
	}

	// Bindless descriptor sets
	const std::array<VkDescriptorBindingFlags, 6> descriptorBindingFlags = {
		VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
		VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
		VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
		VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
		VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
		VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
	};
	const std::array<VkDescriptorType, 6> descriptorTypes = {
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
		VK_DESCRIPTOR_TYPE_SAMPLER,
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
		VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR
	};
	const std::array<uint32_t, 6> descriptorCounts = {
		MAX_UBO_DESCRIPTORS,
		MAX_TEXTURE_DESCRIPTORS,
		MAX_SAMPLER_DESCRIPTORS,
		MAX_STORAGE_BUFFERS,
		MAX_RW_TEXTURE_DESCRIPTORS,
		MAX_RAY_TRACING_TLASES
	};
	std::array<VkDescriptorSetLayoutBinding, 6> descriptorBindings = {};

	for (uint32_t i = 0; i < descriptorBindings.size(); ++i) {
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
	// Ray Tracing properties
	VkPhysicalDeviceProperties2 deviceProperties2 = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
		.pNext = &m_RTProperties
	};
	
	vkGetPhysicalDeviceProperties2(device, &deviceProperties2);

	VkPhysicalDeviceFeatures deviceFeatures;
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

	if (m_EnableValidationLayers) {
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
	const VkSurfaceFormatKHR surfaceFormat = {
		.format = to_vk_format(internalState->info.format),
		.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
	};

	const SwapChainSupportInfo supportInfo = query_swapchain_support(m_PhysicalDevice);

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

	uint32_t imageCount = supportInfo.capabilities.minImageCount + 1;

	// Note: maxImageCount = 0 means that there is no maximum
	if (supportInfo.capabilities.maxImageCount > 0 && imageCount > supportInfo.capabilities.maxImageCount) {
		imageCount = supportInfo.capabilities.maxImageCount;
	}

	auto* oldSwapchain = internalState->swapChain;

	VkSwapchainCreateInfoKHR createInfo = {
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = m_Surface,
		.minImageCount = imageCount,
		.imageFormat = surfaceFormat.format,
		.imageColorSpace = surfaceFormat.colorSpace,
		.imageExtent = extent,
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.preTransform = supportInfo.capabilities.currentTransform,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode = VK_PRESENT_MODE_FIFO_KHR // V-Sync: always guaranteed to be supported
	};

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
		const VkImageViewCreateInfo imageViewInfo = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = internalState->images[i],
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

		if (internalState->imageViews[i] != VK_NULL_HANDLE) {
			m_DestructionHandler.imageViews.push_back({ internalState->imageViews[i], m_DestructionHandler.frameCount });
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

		const VkPipelineShaderStageCreateInfo shaderStageInfo = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = internalShader->shaderModule,
			.pName = "main",
			.pSpecializationInfo = nullptr
		};

		shaderStages.push_back(shaderStageInfo);
	}

	// Pixel shader
	if (info.pixelShader != nullptr) {
		auto internalShader = to_internal(*info.pixelShader);

		const VkPipelineShaderStageCreateInfo shaderStageInfo = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = internalShader->shaderModule,
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
	std::vector<VkPipelineColorBlendAttachmentState> colorBlendStates = {};
	for (uint32_t i = 0; i < info.numRenderTargets; ++i) {
		const BlendState::RenderTargetBlendState& blendState = info.blendState.renderTargetBlendStates[i];

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
		.depthWriteEnable = info.depthStencilState.depthWriteMask == DepthWriteMask::ZERO ? VK_FALSE : VK_TRUE,
		.depthCompareOp = to_vk_comparison_func(info.depthStencilState.depthFunction),
		.depthBoundsTestEnable = VK_FALSE,
		.stencilTestEnable = VK_FALSE,
		.minDepthBounds = 0.0f,
		.maxDepthBounds = 1.0f
	};

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

	// Bind flags
	if (has_flag(info.bindFlags, BindFlag::VERTEX_BUFFER)) {
		bufferInfo.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	}
	else if (has_flag(info.bindFlags, BindFlag::INDEX_BUFFER)) {
		bufferInfo.usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	}
	else if (has_flag(info.bindFlags, BindFlag::UNIFORM_BUFFER)) {
		bufferInfo.usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	}

	// Misc flags
	if (has_flag(info.miscFlags, MiscFlag::BUFFER_STRUCTURED)) {
		bufferInfo.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	}
	if (has_flag(info.miscFlags, MiscFlag::RAY_TRACING)) {
		bufferInfo.usage |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
		bufferInfo.usage |= VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR;
	}

	// TODO: Check if supported
	bufferInfo.usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

	if (vkCreateBuffer(m_Impl->m_Device, &bufferInfo, nullptr, &internalState->buffer) != VK_SUCCESS) {
		throw std::runtime_error("VULKAN ERROR: Failed to create buffer!");
	}

	// TODO: INVESTIGATE
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
	const VkMemoryAllocateFlagsInfo allocFlagsInfo = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
		.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT
	};

	const VkMemoryAllocateInfo allocInfo = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext = &allocFlagsInfo,
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

	// Buffer device address
	if (bufferInfo.usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
		VkBufferDeviceAddressInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
		info.buffer = internalState->buffer;
		internalState->address = vkGetBufferDeviceAddress(m_Impl->m_Device, &info);
	}

	// Descriptors
	if (has_flag(info.bindFlags, BindFlag::UNIFORM_BUFFER)) {
		internalState->descriptor.init_ubo(m_Impl, internalState->buffer);
	}
	else if (has_flag(info.miscFlags, MiscFlag::BUFFER_STRUCTURED)) {
		internalState->descriptor.init_storage_buffer(m_Impl, internalState->buffer);
	}
}

void GFXDevice_Vulkan::create_shader(ShaderStage stage, const std::string& path, Shader& shader) {
	auto internalState = std::make_shared<Shader_Vulkan>();
	internalState->destructionHandler = &m_Impl->m_DestructionHandler;

	shader.internalState = internalState;

	// Load shader code
	const std::string fullPath = ENGINE_RES_DIR + path;
	std::ifstream file(fullPath, std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
		throw std::runtime_error("VULKAN ERROR: Failed to open SPIRV shader file.");
	}

	const size_t fileSize = static_cast<size_t>(file.tellg());
	internalState->shaderCode.resize(fileSize);

	file.seekg(0);
	file.read(reinterpret_cast<char*>(internalState->shaderCode.data()), fileSize);
	file.close();

	// Create shader module
	const VkShaderModuleCreateInfo shaderModuleInfo = {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = internalState->shaderCode.size(),
		.pCode = reinterpret_cast<uint32_t*>(internalState->shaderCode.data())
	};

	if (vkCreateShaderModule(
		m_Impl->m_Device,
		&shaderModuleInfo,
		nullptr,
		&internalState->shaderModule) != VK_SUCCESS) {
		throw std::runtime_error("VULKAN ERROR: Failed to create shader module!");
	}
}

void GFXDevice_Vulkan::create_texture(const TextureInfo& info, Texture& texture, const SubresourceData* data) {
	//assert(data != nullptr);

	auto internalState = std::make_shared<Impl::Texture_Vulkan>();
	internalState->destructionHandler = &m_Impl->m_DestructionHandler;

	texture.internalState = internalState;
	texture.info = info;
	texture.type = Resource::Type::TEXTURE;
	texture.mappedData = nullptr;
	texture.mappedSize = 0;

	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D; // TODO: Only 2d textures supported right now
	imageInfo.extent.width = info.width;
	imageInfo.extent.height = info.height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1; // Also hardcoded, look into it
	imageInfo.arrayLayers = 1;
	imageInfo.format = to_vk_format(info.format);
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.flags = 0;
	imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	// TODO: Not valid for transient attachments ^^
	VkAccessFlags2 resourceState = 0;
	VkImageLayout targetLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	// TODO: Setting target layout this way is a bit confusing
	if (has_flag(info.bindFlags, BindFlag::SHADER_RESOURCE)) {
		imageInfo.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
		resourceState = VK_ACCESS_2_SHADER_READ_BIT;
		targetLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}
	if (has_flag(info.bindFlags, BindFlag::UNORDERED_ACCESS)) {
		imageInfo.usage |= VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		targetLayout = VK_IMAGE_LAYOUT_GENERAL;
	}

	if (has_flag(info.bindFlags, BindFlag::RENDER_TARGET)) {
		imageInfo.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		resourceState |= VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;
		resourceState |= VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
		targetLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	}
	else if (has_flag(info.bindFlags, BindFlag::DEPTH_STENCIL)) {
		imageInfo.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		resourceState |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
		resourceState |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		targetLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL; // TODO: might be wrong
	}

	// NOTE: Usage default is the only allowed usage
	if (info.usage != Usage::DEFAULT) {
		throw std::runtime_error("ENGINE ERROR: Invalid usage, DEFAULT must be used for textures!");
	}

	if (vkCreateImage(m_Impl->m_Device, &imageInfo, nullptr, &internalState->image) != VK_SUCCESS) {
		throw std::runtime_error("VULKAN ERROR: Failed to create image!");
	}

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(m_Impl->m_Device, internalState->image, &memRequirements);

	const VkMemoryAllocateInfo allocInfo = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = memRequirements.size,
		.memoryTypeIndex = m_Impl->find_memory_type(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
	};

	if (vkAllocateMemory(m_Impl->m_Device, &allocInfo, nullptr, &internalState->imageMemory) != VK_SUCCESS) {
		throw std::runtime_error("VULKAN ERROR: Failed to allocate image memory!");
	}

	vkBindImageMemory(m_Impl->m_Device, internalState->image, internalState->imageMemory, 0);

	// TODO: Handle depth and other image aspects in a better manner
	const VkImageAspectFlags aspectFlag = is_depth_format(info.format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
	const VkImageViewCreateInfo imageViewInfo = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.image = internalState->image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = imageInfo.format,
		.subresourceRange = {
			.aspectMask = aspectFlag,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1
		}
	};

	if (vkCreateImageView(m_Impl->m_Device, &imageViewInfo, nullptr, &internalState->imageView) != VK_SUCCESS) {
		throw std::runtime_error("VULKAN ERROR: Failed to create image view.");
	}

	if (data != nullptr && data->data != nullptr) {
		Buffer stagingBuffer{};
		BufferInfo stagingBufferInfo{};
		stagingBufferInfo.size = static_cast<uint64_t>(data->rowPitch * info.height);
		stagingBufferInfo.usage = Usage::UPLOAD;

		create_buffer(stagingBufferInfo, stagingBuffer, data->data);
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
			vk_helpers::ImageTransitionInfo transitionInfo{};
			transitionInfo.image = internalState->image;
			transitionInfo.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			transitionInfo.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			transitionInfo.srcAccessMask = 0;
			transitionInfo.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
			transitionInfo.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
			transitionInfo.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
			transitionInfo.aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;

			vk_helpers::transition_image_layout(transitionInfo, tempCommandBuffer);

			VkBufferImageCopy copyRegion = {};
			copyRegion.bufferOffset = 0;
			copyRegion.bufferRowLength = 0;
			copyRegion.bufferImageHeight = 0;
			copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			copyRegion.imageSubresource.mipLevel = 0;
			copyRegion.imageSubresource.baseArrayLayer = 0;
			copyRegion.imageSubresource.layerCount = 1;
			copyRegion.imageOffset = { 0, 0, 0 };
			copyRegion.imageExtent = { info.width, info.height, 1 };

			vkCmdCopyBufferToImage(
				tempCommandBuffer,
				internalStagingBuffer->buffer,
				internalState->image,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1,
				&copyRegion
			);

			transitionInfo.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			transitionInfo.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			transitionInfo.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
			transitionInfo.dstAccessMask = resourceState;
			transitionInfo.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
			transitionInfo.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

			vk_helpers::transition_image_layout(transitionInfo, tempCommandBuffer);
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
	else {
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
			vk_helpers::ImageTransitionInfo transitionInfo{};
			transitionInfo.image = internalState->image;
			transitionInfo.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			transitionInfo.newLayout = targetLayout;
			transitionInfo.srcAccessMask = 0;
			transitionInfo.dstAccessMask = resourceState;
			transitionInfo.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
			transitionInfo.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
			transitionInfo.aspectFlags = is_depth_format(info.format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

			vk_helpers::transition_image_layout(transitionInfo, tempCommandBuffer);
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

	// Create resource descriptor
	if (has_flag(info.bindFlags, BindFlag::SHADER_RESOURCE)) {
		internalState->descriptor.init_texture(m_Impl, internalState->imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	}
	if (has_flag(info.bindFlags, BindFlag::UNORDERED_ACCESS)) {
		internalState->uavDescriptor.init_rw_texture(m_Impl, internalState->imageView, VK_IMAGE_LAYOUT_GENERAL);
	}
}

void GFXDevice_Vulkan::create_sampler(const SamplerInfo& info, Sampler& sampler) {
	auto internalState = std::make_shared<Sampler_Vulkan>();
	internalState->destructionHandler = &m_Impl->m_DestructionHandler;

	sampler.info = info;
	sampler.internalState = internalState;

	VkPhysicalDeviceProperties deviceProperties = {};
	vkGetPhysicalDeviceProperties(m_Impl->m_PhysicalDevice, &deviceProperties);

	VkSamplerCreateInfo samplerInfo = {
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.mipLodBias = info.mipLODBias,
		.anisotropyEnable = VK_TRUE,
		.maxAnisotropy = deviceProperties.limits.maxSamplerAnisotropy,
		.minLod = info.minLOD,
		.maxLod = info.maxLOD,
		.unnormalizedCoordinates = VK_FALSE
	};

	switch (info.filter) {
	case Filter::MIN_MAG_MIP_POINT:
	case Filter::MINIMUM_MIN_MAG_MIP_POINT:
	case Filter::MAXIMUM_MIN_MAG_MIP_POINT:
		samplerInfo.minFilter = VK_FILTER_NEAREST;
		samplerInfo.magFilter = VK_FILTER_NEAREST;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		samplerInfo.anisotropyEnable = VK_FALSE;
		samplerInfo.compareEnable = VK_FALSE;
		break;
	case Filter::MIN_MAG_POINT_MIP_LINEAR:
	case Filter::MINIMUM_MIN_MAG_POINT_MIP_LINEAR:
	case Filter::MAXIMUM_MIN_MAG_POINT_MIP_LINEAR:
		samplerInfo.minFilter = VK_FILTER_NEAREST;
		samplerInfo.magFilter = VK_FILTER_NEAREST;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.anisotropyEnable = VK_FALSE;
		samplerInfo.compareEnable = VK_FALSE;
		break;
	case Filter::MIN_POINT_MAG_LINEAR_MIP_POINT:
	case Filter::MINIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT:
	case Filter::MAXIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT:
		samplerInfo.minFilter = VK_FILTER_NEAREST;
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		samplerInfo.anisotropyEnable = VK_FALSE;
		samplerInfo.compareEnable = VK_FALSE;
		break;
	case Filter::MIN_POINT_MAG_MIP_LINEAR:
	case Filter::MINIMUM_MIN_POINT_MAG_MIP_LINEAR:
	case Filter::MAXIMUM_MIN_POINT_MAG_MIP_LINEAR:
		samplerInfo.minFilter = VK_FILTER_NEAREST;
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.anisotropyEnable = VK_FALSE;
		samplerInfo.compareEnable = VK_FALSE;
		break;
	case Filter::MIN_LINEAR_MAG_MIP_POINT:
	case Filter::MINIMUM_MIN_LINEAR_MAG_MIP_POINT:
	case Filter::MAXIMUM_MIN_LINEAR_MAG_MIP_POINT:
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.magFilter = VK_FILTER_NEAREST;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		samplerInfo.anisotropyEnable = VK_FALSE;
		samplerInfo.compareEnable = VK_FALSE;
		break;
	case Filter::MIN_LINEAR_MAG_POINT_MIP_LINEAR:
	case Filter::MINIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR:
	case Filter::MAXIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR:
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.magFilter = VK_FILTER_NEAREST;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.anisotropyEnable = VK_FALSE;
		samplerInfo.compareEnable = VK_FALSE;
		break;
	case Filter::MIN_MAG_LINEAR_MIP_POINT:
	case Filter::MINIMUM_MIN_MAG_LINEAR_MIP_POINT:
	case Filter::MAXIMUM_MIN_MAG_LINEAR_MIP_POINT:
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		samplerInfo.anisotropyEnable = VK_FALSE;
		samplerInfo.compareEnable = VK_FALSE;
		break;
	case Filter::MIN_MAG_MIP_LINEAR:
	case Filter::MINIMUM_MIN_MAG_MIP_LINEAR:
	case Filter::MAXIMUM_MIN_MAG_MIP_LINEAR:
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.anisotropyEnable = VK_FALSE;
		samplerInfo.compareEnable = VK_FALSE;
		break;
	case Filter::ANISOTROPIC:
	case Filter::MINIMUM_ANISOTROPIC:
	case Filter::MAXIMUM_ANISOTROPIC:
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.anisotropyEnable = VK_TRUE;
		samplerInfo.maxAnisotropy = std::min(16.0f, std::max(1.0f, static_cast<float>(info.maxAnisotropy)));
		samplerInfo.compareEnable = VK_FALSE;
		break;
	case Filter::COMPARISON_MIN_MAG_MIP_POINT:
		samplerInfo.minFilter = VK_FILTER_NEAREST;
		samplerInfo.magFilter = VK_FILTER_NEAREST;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		samplerInfo.anisotropyEnable = VK_FALSE;
		samplerInfo.compareEnable = VK_TRUE;
		break;
	case Filter::COMPARISON_MIN_MAG_POINT_MIP_LINEAR:
		samplerInfo.minFilter = VK_FILTER_NEAREST;
		samplerInfo.magFilter = VK_FILTER_NEAREST;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.anisotropyEnable = VK_FALSE;
		samplerInfo.compareEnable = VK_TRUE;
		break;
	case Filter::COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT:
		samplerInfo.minFilter = VK_FILTER_NEAREST;
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		samplerInfo.anisotropyEnable = VK_FALSE;
		samplerInfo.compareEnable = VK_TRUE;
		break;
	case Filter::COMPARISON_MIN_POINT_MAG_MIP_LINEAR:
		samplerInfo.minFilter = VK_FILTER_NEAREST;
		samplerInfo.magFilter = VK_FILTER_NEAREST;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		samplerInfo.anisotropyEnable = VK_FALSE;
		samplerInfo.compareEnable = VK_TRUE;
		break;
	case Filter::COMPARISON_MIN_LINEAR_MAG_MIP_POINT:
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.magFilter = VK_FILTER_NEAREST;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		samplerInfo.anisotropyEnable = VK_FALSE;
		samplerInfo.compareEnable = VK_TRUE;
		break;
	case Filter::COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR:
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.magFilter = VK_FILTER_NEAREST;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.anisotropyEnable = VK_FALSE;
		samplerInfo.compareEnable = VK_TRUE;
		break;
	case Filter::COMPARISON_MIN_MAG_LINEAR_MIP_POINT:
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		samplerInfo.anisotropyEnable = VK_FALSE;
		samplerInfo.compareEnable = VK_TRUE;
		break;
	case Filter::COMPARISON_MIN_MAG_MIP_LINEAR:
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.anisotropyEnable = VK_FALSE;
		samplerInfo.compareEnable = VK_TRUE;
		break;
	case Filter::COMPARISON_ANISOTROPIC:
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.anisotropyEnable = VK_TRUE;
		samplerInfo.compareEnable = VK_TRUE;
		break;
	default:
		samplerInfo.minFilter = VK_FILTER_NEAREST;
		samplerInfo.magFilter = VK_FILTER_NEAREST;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		samplerInfo.anisotropyEnable = VK_FALSE;
		samplerInfo.compareEnable = VK_FALSE;
		break;
	}

	samplerInfo.addressModeU = to_vk_texture_address_mode(info.addressU);
	samplerInfo.addressModeV = to_vk_texture_address_mode(info.addressV);
	samplerInfo.addressModeW = to_vk_texture_address_mode(info.addressW);
	samplerInfo.compareOp = to_vk_comparison_func(info.comparisonFunc);
	samplerInfo.borderColor = to_vk_sampler_border_color(info.borderColor);

	if (vkCreateSampler(m_Impl->m_Device, &samplerInfo, nullptr, &internalState->sampler) != VK_SUCCESS) {
		throw std::runtime_error("VULKAN ERROR: Failed to create sampler!");
	}

	// Create sampler descriptor
	VkDescriptorImageInfo imageInfo{};
	imageInfo.sampler = internalState->sampler;

	VkWriteDescriptorSet write{};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.pNext = nullptr;
	write.dstSet = m_Impl->m_ResourceDescriptorSet;
	write.dstBinding = Impl::SAMPLER_BINDING;
	write.dstArrayElement = 0;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
	write.pImageInfo = &imageInfo;

	vkUpdateDescriptorSets(
		m_Impl->m_Device,
		1,
		&write,
		0,
		nullptr
	);
}

// -------------------------------- Ray Tracing --------------------------------
void GFXDevice_Vulkan::create_rtas(const RTASInfo& rtasInfo, RTAS& rtas) {
	auto internalState = std::make_shared<RTAS_Vulkan>();
	internalState->info = rtasInfo;
	internalState->destructionHandler = &m_Impl->m_DestructionHandler;

	rtas.internalState = internalState;
	rtas.info = rtasInfo;
	rtas.type = Resource::Type::RAYTRACING_AS;

	// Geometry info
	VkAccelerationStructureBuildGeometryInfoKHR& buildInfo = internalState->buildInfo;
	buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;

	switch (rtasInfo.type) {
	case RTASType::BLAS:
		{
			buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

			for (const auto& geometry : rtasInfo.blas.geometries) {
				VkAccelerationStructureGeometryKHR& vkGeometry = internalState->geometries.emplace_back();
				uint32_t& primitiveCount = internalState->primitiveCounts.emplace_back();

				vkGeometry = {};
				vkGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
				vkGeometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR; // TODO: For now we only support triangles as the geometry type
				//vkGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

				// Triangle geometry
				auto& vkTriangles = vkGeometry.geometry.triangles;
				vkTriangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
				vkTriangles.vertexFormat = to_vk_format(geometry.triangles.vertexFormat);
				vkTriangles.vertexData.deviceAddress = m_Impl->to_internal(*geometry.triangles.vertexBuffer)->address +
					geometry.triangles.vertexByteOffset;
				vkTriangles.vertexStride = static_cast<uint64_t>(geometry.triangles.vertexStride);
				vkTriangles.maxVertex = geometry.triangles.vertexCount - 1; // TODO: Not sure if -1 is needed
				vkTriangles.indexType = VK_INDEX_TYPE_UINT32;
				vkTriangles.indexData.deviceAddress = m_Impl->to_internal(*geometry.triangles.indexBuffer)->address +
					geometry.triangles.indexOffset * sizeof(uint32_t);

				primitiveCount = geometry.triangles.indexCount / 3;
			}
		}
		break;
	case RTASType::TLAS:
		{
			buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

			VkAccelerationStructureGeometryKHR& vkGeometry = internalState->geometries.emplace_back();
			uint32_t& primitiveCount = internalState->primitiveCounts.emplace_back();

			vkGeometry = {};
			vkGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
			vkGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
			vkGeometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
			vkGeometry.geometry.instances.arrayOfPointers = VK_FALSE;

			auto internalInstanceBuffer = m_Impl->to_internal(*rtas.info.tlas.instanceBuffer);
			vkGeometry.geometry.instances.data.deviceAddress = internalInstanceBuffer->address;

			primitiveCount = rtasInfo.tlas.numInstances;
		}
		break;
	}

	buildInfo.geometryCount = static_cast<uint32_t>(internalState->geometries.size());
	buildInfo.pGeometries = internalState->geometries.data();

	internalState->sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

	// Memory requirements
	vkGetAccelerationStructureBuildSizesKHR(
		m_Impl->m_Device,
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&internalState->buildInfo,
		internalState->primitiveCounts.data(),
		&internalState->sizeInfo
	);

	// Create the acceleration structure
	VkBufferCreateInfo bufferInfo = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.flags = 0,
		.size = internalState->sizeInfo.accelerationStructureSize,
		.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE
	};

	if (vkCreateBuffer(m_Impl->m_Device, &bufferInfo, nullptr, &internalState->asBuffer) != VK_SUCCESS) {
		throw std::runtime_error("VULKAN ERROR: Failed to create acceleration structure buffer!");
	}

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(m_Impl->m_Device, internalState->asBuffer, &memRequirements);

	uint32_t memoryType = m_Impl->find_memory_type(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	const VkMemoryAllocateFlagsInfo allocFlagsInfo = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
		.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT
	};

	VkMemoryAllocateInfo allocInfo = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext = &allocFlagsInfo,
		.allocationSize = memRequirements.size,
		.memoryTypeIndex = memoryType
	};

	if (vkAllocateMemory(m_Impl->m_Device, &allocInfo, nullptr, &internalState->asBufferMemory) != VK_SUCCESS) {
		throw std::runtime_error("VULKAN ERROR: Failed to allocate acceleration structure memory!");
	}

	if (vkBindBufferMemory(m_Impl->m_Device, internalState->asBuffer, internalState->asBufferMemory, 0) != VK_SUCCESS) {
		throw std::runtime_error("VULKAN ERROR: Failed to bind acceleration structure buffer memory!");
	}

	VkAccelerationStructureCreateInfoKHR& createInfo = internalState->createInfo;
	createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
	createInfo.buffer = internalState->asBuffer;
	createInfo.size = internalState->sizeInfo.accelerationStructureSize;
	createInfo.type = internalState->buildInfo.type;

	VkResult res = vkCreateAccelerationStructureKHR(
		m_Impl->m_Device,
		&createInfo,
		nullptr,
		&internalState->as
	);

	if (res != VK_SUCCESS) {
		throw std::runtime_error("VULKAN ERROR: Failed to create ray tracing acceleration structure!");
	}

	// Create the scratch buffer
	const BufferInfo scratchBufferInfo = {
		.size = internalState->sizeInfo.buildScratchSize,
		.usage = Usage::DEFAULT,
		.bindFlags = BindFlag::SHADER_RESOURCE,
		.miscFlags = MiscFlag::BUFFER_STRUCTURED
	};

	create_buffer(scratchBufferInfo, internalState->scratchBuffer, nullptr);

	// Device address for acceleration structure
	const VkAccelerationStructureDeviceAddressInfoKHR deviceAddressInfo = {
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
		.accelerationStructure = internalState->as
	};

	internalState->asDeviceAddress = vkGetAccelerationStructureDeviceAddressKHR(
		m_Impl->m_Device,
		&deviceAddressInfo
	);
}

void GFXDevice_Vulkan::create_rt_instance_buffer(Buffer& buffer, uint32_t numBLASes) {
	const BufferInfo instanceBufferInfo = {
		.size = numBLASes * sizeof(VkAccelerationStructureInstanceKHR),
		.stride = sizeof(VkAccelerationStructureInstanceKHR),
		.usage = Usage::UPLOAD,
		.bindFlags = BindFlag::SHADER_RESOURCE,
		.miscFlags = MiscFlag::BUFFER_STRUCTURED | MiscFlag::RAY_TRACING,
		.persistentMap = true
	};

	create_buffer(instanceBufferInfo, buffer, nullptr);
}

void GFXDevice_Vulkan::create_rt_pipeline(const RTPipelineInfo& info, RTPipeline& pipeline) {
	assert(info.rayGenShader != nullptr && "Ray-generation shader required");
	assert(info.missShader != nullptr && "Miss shader required!");
	assert(info.closestHitShader != nullptr && "Closest-Hit shader required!");

	auto internalState = std::make_shared<RTPipeline_Vulkan>();
	internalState->info = info;
	internalState->destructionHandler = &m_Impl->m_DestructionHandler;

	pipeline.info = info;
	pipeline.internalState = internalState;

	std::vector<VkPipelineShaderStageCreateInfo> shaderStages = {};

	// Ray-generation shader
	{
		auto internalShader = to_internal(*info.rayGenShader);

		VkPipelineShaderStageCreateInfo& shaderStageInfo = shaderStages.emplace_back();
		shaderStageInfo = {};
		shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStageInfo.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
		shaderStageInfo.module = internalShader->shaderModule;
		shaderStageInfo.pName = "main";
		shaderStageInfo.pSpecializationInfo = nullptr;
	}

	// Miss shader
	{
		auto internalShader = to_internal(*info.missShader);

		VkPipelineShaderStageCreateInfo& shaderStageInfo = shaderStages.emplace_back();
		shaderStageInfo = {};
		shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStageInfo.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
		shaderStageInfo.module = internalShader->shaderModule;
		shaderStageInfo.pName = "main";
		shaderStageInfo.pSpecializationInfo = nullptr;
	}

	// Closest-hit shader
	{
		auto internalShader = to_internal(*info.closestHitShader);

		VkPipelineShaderStageCreateInfo& shaderStageInfo = shaderStages.emplace_back();
		shaderStageInfo = {};
		shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStageInfo.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
		shaderStageInfo.module = internalShader->shaderModule;
		shaderStageInfo.pName = "main";
		shaderStageInfo.pSpecializationInfo = nullptr;
	}

	// Shader groups
	std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups = {};
	groups.reserve(info.shaderGroups.size());

	for (size_t i = 0; i < info.shaderGroups.size(); ++i) {
		const RTShaderGroup& shaderGroup = info.shaderGroups[i];

		VkRayTracingShaderGroupCreateInfoKHR& vkGroup = groups.emplace_back();
		vkGroup = {};
		vkGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;

		switch (shaderGroup.type) {
		case RTShaderGroup::Type::GENERAL:
			vkGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
			break;
		case RTShaderGroup::Type::PROCEDURAL:
			vkGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR;
			break;
		case RTShaderGroup::Type::TRIANGLES:
			vkGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
			break;
		}

		vkGroup.generalShader = shaderGroup.generalShader;
		vkGroup.closestHitShader = shaderGroup.closestHitShader;
		vkGroup.anyHitShader = shaderGroup.anyHitShader;
		vkGroup.intersectionShader = shaderGroup.intersectionShader;
	}

	// Bindless descriptors
	const VkDescriptorSetLayout descriptorSetLayouts[1] = {
		m_Impl->m_ResourceDescriptorSetLayout
	};

	const VkPushConstantRange pushConstantRange = {
		.stageFlags = VK_SHADER_STAGE_ALL,
		.offset = 0,
		.size = 128
	};

	// Create pipeline layout
	const VkPipelineLayoutCreateInfo layoutInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = descriptorSetLayouts,
		.pushConstantRangeCount = 1,
		.pPushConstantRanges = &pushConstantRange
	};

	VkResult res = vkCreatePipelineLayout(
		m_Impl->m_Device,
		&layoutInfo,
		nullptr,
		&internalState->psoLayout
	);

	if (res != VK_SUCCESS) {
		throw std::runtime_error("VULKAN ERROR: Failed to create ray tracing pipeline layout!");
	}

	// Create pipeline
	const VkRayTracingPipelineCreateInfoKHR pipelineInfo = {
		.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
		.stageCount = static_cast<uint32_t>(shaderStages.size()),
		.pStages = shaderStages.data(),
		.groupCount = static_cast<uint32_t>(groups.size()),
		.pGroups = groups.data(),
		.maxPipelineRayRecursionDepth = info.maxRayRecursionDepth,
		.layout = internalState->psoLayout,
		.basePipelineHandle = nullptr,
		.basePipelineIndex = 0
	};

	res = vkCreateRayTracingPipelinesKHR(
		m_Impl->m_Device,
		nullptr,
		nullptr,
		1,
		&pipelineInfo,
		nullptr,
		&internalState->pso
	);

	if (res != VK_SUCCESS) {
		throw std::runtime_error("VULKAN ERROR: Failed to create ray tracing pipeline!");
	}
}

void GFXDevice_Vulkan::create_shader_binding_table(const RTPipeline& pipeline, uint32_t groupID, ShaderBindingTable& sbt) {
	const auto internalPipeline = to_internal(pipeline);
	const uint32_t handleSize = m_Impl->m_RTProperties.shaderGroupHandleSize;
	const uint32_t handleSizeAligned = sr::math::align_to(handleSize, m_Impl->m_RTProperties.shaderGroupHandleAlignment);

	const BufferInfo sbtBufferInfo = {
		.size = handleSize * 1, // TODO: ONE HANDLE ONLY, needs fixing
		.stride = handleSizeAligned, // NOTE: The stride for raygen is actually the size itself.
									 // However, in this case it doesn't matter since we account
									 // for this in the dispatch_rays function.
		.usage = Usage::UPLOAD,
		.miscFlags = MiscFlag::RAY_TRACING,
		.persistentMap = true
	};
	create_buffer(sbtBufferInfo, sbt.buffer, nullptr);

	std::vector<uint8_t> shaderHandleStorage(handleSize);
	const VkResult res = vkGetRayTracingShaderGroupHandlesKHR(
		m_Impl->m_Device,
		internalPipeline->pso,
		groupID,
		1,
		handleSize,
		shaderHandleStorage.data()
	);

	if (res != VK_SUCCESS) {
		throw std::runtime_error("VULKAN ERROR: Failed to get ray tracing shader group handles!");
	}

	std::memcpy(sbt.buffer.mappedData, shaderHandleStorage.data(), handleSize);
}

void GFXDevice_Vulkan::write_blas_instance(const RTTLAS::BLASInstance& instance, void* dst) {
	assert(instance.blasResource != nullptr);
	auto internalBLAS = to_internal(*reinterpret_cast<const RTAS*>(instance.blasResource));

	const VkAccelerationStructureInstanceKHR vkInstance = {
		.transform = *(VkTransformMatrixKHR*)&instance.transform,
		.instanceCustomIndex = instance.instanceID,
		.mask = instance.instanceMask,
		.instanceShaderBindingTableRecordOffset = instance.instanceContributionHitGroupIndex, // TODO: Research
		.flags = instance.flags,
		.accelerationStructureReference = internalBLAS->asDeviceAddress
	};

	std::memcpy(dst, &vkInstance, sizeof(vkInstance));
}

void GFXDevice_Vulkan::build_rtas(RTAS& rtas, const CommandList& cmdList) {
	auto internalRTAS = to_internal(rtas);
	auto internalCmdList = to_internal(cmdList);

	VkAccelerationStructureBuildGeometryInfoKHR info = internalRTAS->buildInfo;
	info.dstAccelerationStructure = internalRTAS->as;
	info.srcAccelerationStructure = nullptr;
	info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	info.scratchData.deviceAddress = m_Impl->to_internal(internalRTAS->scratchBuffer)->address;

	std::vector<VkAccelerationStructureBuildRangeInfoKHR> buildRanges = {};
	buildRanges.reserve(info.geometryCount);

	switch (rtas.info.type) {
	case RTASType::BLAS:
		for (const auto& geometry : rtas.info.blas.geometries) {
			// TODO: Only works for triangles for now
			auto& buildRange = buildRanges.emplace_back();
			buildRange = {};
			buildRange.primitiveCount = geometry.triangles.indexCount / 3;
			buildRange.primitiveOffset = 0;
		}
		break;
	case RTASType::TLAS:
		{
			auto& buildRange = buildRanges.emplace_back();
			buildRange = {};
			buildRange.primitiveCount = static_cast<uint32_t>(rtas.info.tlas.numInstances);
			buildRange.primitiveOffset = 0;
		}
		break;
	}

	VkAccelerationStructureBuildRangeInfoKHR* pRangeInfo = buildRanges.data();

	// Temporary command list
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

	// Build the acceleration structure
	vkCmdBuildAccelerationStructuresKHR(
		tempCommandBuffer,
		1,
		&info,
		&pRangeInfo
	);

	vkEndCommandBuffer(tempCommandBuffer);

	const VkSubmitInfo submitInfo = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount = 1,
		.pCommandBuffers = &tempCommandBuffer
	};

	vkQueueSubmit(m_Impl->m_GraphicsQueue, 1, &submitInfo, nullptr);
	vkQueueWaitIdle(m_Impl->m_GraphicsQueue);
	vkFreeCommandBuffers(m_Impl->m_Device, m_Impl->m_CommandPool, 1, &tempCommandBuffer);

	// Create TLAS descriptor
	// TODO: This is extremely hacky since we create it from here and also
	// assume that this function will only ever be called once for TLAS, i.e.
	// we assume that only 1 TLAS descriptor will ever exist
	if (rtas.info.type == RTASType::TLAS) {
		const VkWriteDescriptorSetAccelerationStructureKHR asDescriptorWrite = {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
			.accelerationStructureCount = 1,
			.pAccelerationStructures = &internalRTAS->as
		};

		const VkWriteDescriptorSet descriptorWrite = {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = &asDescriptorWrite,
			.dstSet = m_Impl->m_ResourceDescriptorSet,
			.dstBinding = Impl::TLAS_BINDING,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
		};

		vkUpdateDescriptorSets(m_Impl->m_Device, 1, &descriptorWrite, 0, nullptr);
	}
}

void GFXDevice_Vulkan::bind_rt_pipeline(const RTPipeline& pipeline, const CommandList& cmdList) {
	auto internalPipeline = to_internal(pipeline);
	auto internalCmdList = to_internal(cmdList);

	VkCommandBuffer currCommandBuffer = internalCmdList->commandBuffers[m_CurrentFrame];

	vkCmdBindPipeline(
		currCommandBuffer,
		VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
		internalPipeline->pso
	);

	vkCmdBindDescriptorSets(
		currCommandBuffer,
		VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
		internalPipeline->psoLayout,
		0,
		1,
		&m_Impl->m_ResourceDescriptorSet,
		0,
		nullptr
	);
}

void GFXDevice_Vulkan::push_rt_constants(const void* data, uint32_t size, const RTPipeline& pipeline, const CommandList& cmdList) {
	assert(data != nullptr);
	assert(size <= 128);

	auto internalCmdList = to_internal(cmdList);
	auto internalPipeline = to_internal(pipeline);

	vkCmdPushConstants(
		internalCmdList->commandBuffers[m_CurrentFrame],
		internalPipeline->psoLayout,
		VK_SHADER_STAGE_ALL,
		0,
		size,
		data
	);
}

void GFXDevice_Vulkan::dispatch_rays(const DispatchRaysInfo& info, const CommandList& cmdList) {
	auto internalCmdList = to_internal(cmdList);

	VkStridedDeviceAddressRegionKHR raygen = {};
	raygen.deviceAddress = m_Impl->to_internal(info.rayGenTable->buffer)->address;
	raygen.deviceAddress += info.rayGenTable->offset;
	raygen.size = info.rayGenTable->size;
	raygen.stride = raygen.size; // raygen specifically must be size == stride

	VkStridedDeviceAddressRegionKHR miss = {};
	miss.deviceAddress = m_Impl->to_internal(info.missTable->buffer)->address;
	miss.deviceAddress += info.missTable->offset;
	miss.size = info.missTable->size;
	miss.stride = info.missTable->stride;

	VkStridedDeviceAddressRegionKHR hitgroup = {};
	hitgroup.deviceAddress = m_Impl->to_internal(info.hitGroupTable->buffer)->address;
	hitgroup.deviceAddress += info.hitGroupTable->offset;
	hitgroup.size = info.hitGroupTable->size;
	hitgroup.stride = info.hitGroupTable->stride;

	VkStridedDeviceAddressRegionKHR callable = {};

	// TODO: Callables?
	vkCmdTraceRaysKHR(
		internalCmdList->commandBuffers[m_CurrentFrame],
		&raygen,
		&miss,
		&hitgroup,
		&callable,
		info.width,
		info.height,
		info.depth
	);
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

void GFXDevice_Vulkan::barrier(const GPUBarrier& barrier, const CommandList& cmdList) {
	auto internalCmdList = to_internal(cmdList);

	switch (barrier.type) {
	case GPUBarrier::Type::IMAGE:
		{
			if (barrier.image.stateBefore == barrier.image.stateAfter) {
				return;
			}

			const TextureInfo& textureInfo = barrier.image.texture->info;
			auto internalTexture = m_Impl->to_internal(*barrier.image.texture);

			const bool isDepth = is_depth_format(barrier.image.texture->info.format);
			const VkImageAspectFlags aspectFlag = isDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

			vk_helpers::ImageTransitionInfo transitionInfo = {
				.image = internalTexture->image,
				.oldLayout = to_vk_resource_state(barrier.image.stateBefore),
				.newLayout = to_vk_resource_state(barrier.image.stateAfter),
				.srcAccessMask = to_vk_resource_access(barrier.image.stateBefore),
				.dstAccessMask = to_vk_resource_access(barrier.image.stateAfter),
				.srcStageMask = to_vk_pipeline_stage(barrier.image.stateBefore),
				.dstStageMask = to_vk_pipeline_stage(barrier.image.stateAfter),
				.aspectFlags = aspectFlag
			};

			vk_helpers::transition_image_layout(transitionInfo, internalCmdList->commandBuffers[m_CurrentFrame]);
		}
		break;
	}
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

void GFXDevice_Vulkan::begin_render_pass(const SwapChain& swapChain, const PassInfo& passInfo, const CommandList& cmdList, bool clear) {
	auto internalSwapChain = to_internal(swapChain);
	auto internalCmdList = to_internal(cmdList);

	// EXTREMELY HACKY SOLUTION (PLS FIX): Since the render graph will only
	// allow clear on the FIRST root pass, we know that when clear is true,
	// it is the first root pass and thus, we acquire a new image.
	if (clear) {
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
	}

	VkCommandBuffer currVkCommandBuffer = internalCmdList->commandBuffers[m_CurrentFrame];

	const vk_helpers::ImageTransitionInfo transitionInfo = {
		.image = internalSwapChain->images[m_CurrentImageIndex],
		.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.srcAccessMask = VK_ACCESS_2_NONE,
		.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
		.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		.aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT
	};
	vk_helpers::transition_image_layout(transitionInfo, currVkCommandBuffer);

	const VkClearValue clearColor = {
		.color = { 0.0f, 0.0f, 0.0f, 1.0f }
	};

	const VkRenderingAttachmentInfo colorAttachmentInfo = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
		.imageView = internalSwapChain->imageViews[m_CurrentImageIndex],
		.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.clearValue = clearColor
	};

	// Depth attachment
	VkRenderingAttachmentInfo depthAttachmentInfo = {};

	if (passInfo.depth != nullptr) {
		auto internalDepthTexture = m_Impl->to_internal(*passInfo.depth);

		const VkClearValue depthClearValue = {
			.depthStencil = {
				.depth = 1.0f,
				.stencil = 0
			}
		};

		depthAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		depthAttachmentInfo.pNext = nullptr;
		depthAttachmentInfo.imageView = internalDepthTexture->imageView;
		depthAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		depthAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		depthAttachmentInfo.clearValue = depthClearValue;
	}

	const VkRect2D area = {
		.offset = { 0, 0 },
		.extent = internalSwapChain->extent
	};

	const VkRenderingInfo renderInfo = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
		.renderArea = area,
		.layerCount = 1,
		.viewMask = 0,
		.colorAttachmentCount = 1,
		.pColorAttachments = &colorAttachmentInfo,
		.pDepthAttachment = passInfo.depth != nullptr ? &depthAttachmentInfo : nullptr,
		.pStencilAttachment = nullptr
	};

	vkCmdBeginRendering(currVkCommandBuffer, &renderInfo);
}

void GFXDevice_Vulkan::begin_render_pass(const PassInfo& passInfo, const CommandList& cmdList) {
	auto internalCmdList = to_internal(cmdList);

	VkCommandBuffer currVkCommandBuffer = internalCmdList->commandBuffers[m_CurrentFrame];
	const VkClearValue clearColor = { { { 0.0f, 0.0f, 0.0f, 1.0f } } };

	std::vector<VkRenderingAttachmentInfo> colorAttachmentInfos(passInfo.numColorAttachments);

	VkRect2D renderArea = {
		.offset = { 0, 0 },
		.extent = { 0, 0 }
	};

	for (size_t i = 0; i < colorAttachmentInfos.size(); ++i) {
		const Texture* attachmentTexture = passInfo.colors[i];
		auto internalAttachmentTexture = m_Impl->to_internal(*attachmentTexture);

		renderArea.extent.width = std::max(renderArea.extent.width, attachmentTexture->info.width);
		renderArea.extent.height = std::max(renderArea.extent.height, attachmentTexture->info.height);

		VkRenderingAttachmentInfo& attachment = colorAttachmentInfos[i];
		attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		attachment.imageView = internalAttachmentTexture->imageView;
		attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // TODO: Investigate this
		attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // ^^
		attachment.clearValue = clearColor;
	}

	// Depth attachment
	VkRenderingAttachmentInfo depthAttachmentInfo = {};
	
	if (passInfo.depth != nullptr) {
		auto internalDepthTexture = m_Impl->to_internal(*passInfo.depth);

		const VkClearValue depthClearValue = {
			.depthStencil = {
				.depth = 1.0f,
				.stencil = 0
			}
		};

		depthAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		depthAttachmentInfo.pNext = nullptr;
		depthAttachmentInfo.imageView = internalDepthTexture->imageView;
		depthAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		depthAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		depthAttachmentInfo.clearValue = depthClearValue;
	}

	const VkRenderingInfo renderInfo = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
		.renderArea = renderArea,
		.layerCount = 1,
		.viewMask = 0,
		.colorAttachmentCount = passInfo.numColorAttachments,
		.pColorAttachments = colorAttachmentInfos.data(),
		.pDepthAttachment = passInfo.depth != nullptr ? &depthAttachmentInfo : nullptr,
		.pStencilAttachment = nullptr
	};

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
		.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
		.aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT
	};
	vk_helpers::transition_image_layout(transitionInfo, currVkCommandBuffer);
}

void GFXDevice_Vulkan::end_render_pass(const CommandList& cmdList) {
	auto internalCmdList = to_internal(cmdList);

	vkCmdEndRendering(internalCmdList->commandBuffers[m_CurrentFrame]);
}

void GFXDevice_Vulkan::submit_command_lists(const SwapChain& swapChain) {
	auto internalSwapChain = to_internal(swapChain);

	const uint32_t tempCmdListCount = static_cast<uint32_t>(m_Impl->m_CmdListCounter);
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

void GFXDevice_Vulkan::draw_instanced(uint32_t vertexCount, uint32_t instanceCount, uint32_t startVertex, uint32_t startInstance, const CommandList& cmdList) {
	auto internalCommandBuffer = to_internal(cmdList);

	vkCmdDraw(
		internalCommandBuffer->commandBuffers[m_CurrentFrame],
		vertexCount,
		instanceCount,
		startVertex,
		startInstance
	);
}

uint32_t GFXDevice_Vulkan::get_descriptor_index(const Resource& resource, SubresourceType type) {
	if (resource.type == Resource::Type::TEXTURE) {
		auto internalTexture = (Impl::Texture_Vulkan*)resource.internalState.get();

		// TODO: Handle subresource types BETTER
		switch (type) {
		case SubresourceType::SRV:
			return internalTexture->descriptor.index;
		case SubresourceType::UAV:
			return internalTexture->uavDescriptor.index;
		}
	}
	else if (resource.type == Resource::Type::BUFFER) {
		auto internalBuffer = (Impl::Buffer_Vulkan*)resource.internalState.get();

		return internalBuffer->descriptor.index;
	}
}

uint64_t GFXDevice_Vulkan::get_bda(const Buffer& buffer) {
	return m_Impl->to_internal(buffer)->address;
}

void GFXDevice_Vulkan::wait_for_gpu() {
	vkDeviceWaitIdle(m_Impl->m_Device);
}
