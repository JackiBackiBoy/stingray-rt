#pragma once

#include "Core/Logger.hpp"
#include "Graphics/GraphicsTypes.hpp"

#include <cassert>
#include <deque>
#include <vector>
#include <volk.h>
#include <vk_mem_alloc.h>

#define SR_VK_CHECK(expr, msg)                                                 \
	do {                                                                       \
		VkResult res = (expr);                                                 \
		if (res != VK_SUCCESS) {                                               \
			SRLOG_ERROR_CAT(SRLOG_CAT_VULKAN, "%s failed: %s (%d)", msg, "vk_res_to_str(res)", res); \
			throw std::runtime_error("Vulkan error: " msg);                    \
		}                                                                      \
	} while (0)

class SRDescriptorHeap_Vulkan {
public:
	SRDescriptorHeap_Vulkan(VkDescriptorType type, uint32_t capacity);
	~SRDescriptorHeap_Vulkan() = default;

	SRDescriptorIndex get_next_index();
	void free_index(SRDescriptorIndex index);

	VkDescriptorType get_type() const { return m_Type; }
	uint32_t get_capacity() const { return m_Capacity; }

private:
	inline void clear_state_bit(SRDescriptorIndex index) {
		m_StateArray[index >> 6ull] &= ~(1ull << (index & 63ull));
	}

	inline void set_state_bit(SRDescriptorIndex index) {
		m_StateArray[index >> 6ull] |= (1ull << (index & 63ull));
	}

	inline bool get_state_bit(SRDescriptorIndex index) const {
		return (m_StateArray[index >> 6ull] & (1ull << (index & 63ull))) != 0ull;
	}

	VkDescriptorType m_Type;
	uint32_t m_Capacity;
	uint32_t m_Size = 0;
	std::vector<SRDescriptorIndex> m_FreeList;
	std::vector<uint64_t> m_StateArray;
};

class SRDestructionHandler_Vulkan {
public:
	SRDestructionHandler_Vulkan(VkDevice device, VkInstance instance, VmaAllocator allocator) :
		m_Device(device), m_Instance(instance), m_Allocator(allocator) {}
	~SRDestructionHandler_Vulkan() {
		update(~0, 0);

		vmaDestroyAllocator(m_Allocator);
		vkDestroyDevice(m_Device, nullptr);
		
		// NOTE: Destroying debug messengers is a special case, because we can
		// not have it inside the update function since it will be called before
		// vkDestroyDevice, meaning that debug messenger will not be alive
		// during device destruction. Thus debug messenger destruction should
		// take place after vkDestroyDevice, but BEFORE vkDestroyInstance.
		while (!m_DebugMessengers.empty()) {
			auto debugMessenger = m_DebugMessengers.front().first;
			auto fn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
				vkGetInstanceProcAddr(m_Instance, "vkDestroyDebugUtilsMessengerEXT"));
			if (fn) {
				fn(m_Instance, debugMessenger, nullptr);
			}
			m_DebugMessengers.pop_front();
		}

		vkDestroyInstance(m_Instance, nullptr);
	}

	void update(uint64_t frameCount, uint32_t bufferCount) {
		const auto destroy = [&](auto& queue, auto destroyFn) {
			while (!queue.empty()) {
				const auto [item, born] = queue.front();

				if (born + bufferCount < frameCount) {
					destroyFn(item);
					queue.pop_front();
					continue;
				}
				break;
			}
		};

		destroy(m_Allocations, [&](VmaAllocation item) {
			vmaFreeMemory(m_Allocator, item);
		});
		destroy(m_Images, [&](std::pair<VkImage, VmaAllocation> item) {
			vmaDestroyImage(m_Allocator, item.first, item.second);
		});
		destroy(m_Semaphores, [&](VkSemaphore item) {
			vkDestroySemaphore(m_Device, item, nullptr);
		});
		destroy(m_Fences, [&](VkFence item) {
			vkDestroyFence(m_Device, item, nullptr);
		});
		destroy(m_CommandPools, [&](VkCommandPool item) {
			vkDestroyCommandPool(m_Device, item, nullptr);
		});
		destroy(m_ImageViews, [&](VkImageView item) {
			vkDestroyImageView(m_Device, item, nullptr);
		});
		destroy(m_Buffers, [&](VkBuffer item) {
			vkDestroyBuffer(m_Device, item, nullptr);
		});
		destroy(m_AccelerationStructures, [&](VkAccelerationStructureKHR item) {
			vkDestroyAccelerationStructureKHR(m_Device, item, nullptr);
		});
		destroy(m_Samplers, [&](VkSampler item) {
			vkDestroySampler(m_Device, item, nullptr);
		});
		destroy(m_DescriptorPools, [&](VkDescriptorPool item) {
			vkDestroyDescriptorPool(m_Device, item, nullptr);
		});
		destroy(m_DescriptorSetLayouts, [&](VkDescriptorSetLayout item) {
			vkDestroyDescriptorSetLayout(m_Device, item, nullptr);
		});
		destroy(m_ShaderModules, [&](VkShaderModule item) {
			vkDestroyShaderModule(m_Device, item, nullptr);
		});
		destroy(m_Pipelines, [&](VkPipeline item) {
			vkDestroyPipeline(m_Device, item, nullptr);
		});
		destroy(m_PipelineLayouts, [&](VkPipelineLayout item) {
			vkDestroyPipelineLayout(m_Device, item, nullptr);
		});
		destroy(m_Swapchains, [&](VkSwapchainKHR item) {
			vkDestroySwapchainKHR(m_Device, item, nullptr);
		});
		destroy(m_Surfaces, [&](VkSurfaceKHR item) {
			vkDestroySurfaceKHR(m_Instance, item, nullptr);
		});

		m_FrameCount = frameCount;
	}

	void enqueue(VmaAllocation item)                { push(m_Allocations, item); }
	void enqueue(VkSemaphore item)                  { push(m_Semaphores, item); }
	void enqueue(VkFence item)                      { push(m_Fences, item); }
	void enqueue(VkCommandPool item)                { push(m_CommandPools, item); }
	void enqueue(VkImage item, VmaAllocation alloc) { push(m_Images, { item, alloc }); }
	void enqueue(VkImageView item)                  { push(m_ImageViews, item); }
	void enqueue(VkBuffer item)                     { push(m_Buffers, item); }
	void enqueue(VkAccelerationStructureKHR item)   { push(m_AccelerationStructures, item); }
	void enqueue(VkSampler item)                    { push(m_Samplers, item); }
	void enqueue(VkDescriptorPool item)             { push(m_DescriptorPools, item); }
	void enqueue(VkDescriptorSetLayout item)        { push(m_DescriptorSetLayouts, item); }
	void enqueue(VkShaderModule item)               { push(m_ShaderModules, item); }
	void enqueue(VkPipeline item)                   { push(m_Pipelines, item); }
	void enqueue(VkPipelineLayout item)             { push(m_PipelineLayouts, item); }
	void enqueue(VkSwapchainKHR item)               { push(m_Swapchains, item); }
	void enqueue(VkSurfaceKHR item)                 { push(m_Surfaces, item); }
	void enqueue(VkDebugUtilsMessengerEXT item)     { push(m_DebugMessengers, item); }

private:
	template<typename T>
	void push(std::deque<std::pair<T, uint64_t>>& dq, T h) {
		dq.emplace_back(h, m_FrameCount);
	}

	VkDevice m_Device = nullptr;
	VkInstance m_Instance = nullptr;
	VmaAllocator m_Allocator = nullptr;
	uint64_t m_FrameCount = 0;

	std::deque<std::pair<VmaAllocation, uint64_t>> m_Allocations;
	std::deque<std::pair<std::pair<VkImage, VmaAllocation>, uint64_t>> m_Images;
	std::deque<std::pair<VkAccelerationStructureKHR, uint64_t>> m_AccelerationStructures;
	std::deque<std::pair<VkCommandPool, uint64_t>> m_CommandPools;
	std::deque<std::pair<VkDescriptorPool, uint64_t>> m_DescriptorPools;
	std::deque<std::pair<VkDescriptorSetLayout, uint64_t>> m_DescriptorSetLayouts;
	std::deque<std::pair<VkFence, uint64_t>> m_Fences;
	std::deque<std::pair<VkImageView, uint64_t>> m_ImageViews;
	std::deque<std::pair<VkPipeline, uint64_t>> m_Pipelines;
	std::deque<std::pair<VkPipelineLayout, uint64_t>> m_PipelineLayouts;
	std::deque<std::pair<VkSampler, uint64_t>> m_Samplers;
	std::deque<std::pair<VkSemaphore, uint64_t>> m_Semaphores;
	std::deque<std::pair<VkShaderModule, uint64_t>> m_ShaderModules;
	std::deque<std::pair<VkSurfaceKHR, uint64_t>> m_Surfaces;
	std::deque<std::pair<VkSwapchainKHR, uint64_t>> m_Swapchains;
	std::deque<std::pair<VkBuffer, uint64_t>> m_Buffers;
	std::deque<std::pair<VkDebugUtilsMessengerEXT, uint64_t>> m_DebugMessengers;
};

struct SRBuffer_Vulkan {
	~SRBuffer_Vulkan() {
		destructionHandler->enqueue(buffer);
		destructionHandler->enqueue(allocation);
	}

	SRDestructionHandler_Vulkan* destructionHandler = nullptr;
	VkBuffer buffer = VK_NULL_HANDLE;
	VmaAllocation allocation = nullptr;
	SRDescriptorIndex uboDescriptor = INVALID_DESCRIPTOR_INDEX;
};

struct SRTexture_Vulkan {
	~SRTexture_Vulkan() {
		destructionHandler->enqueue(image, allocation);
		destructionHandler->enqueue(imageView);
	}

	SRDestructionHandler_Vulkan* destructionHandler = nullptr;
	VkImage image = VK_NULL_HANDLE;
	VkImageView imageView = VK_NULL_HANDLE;
	VmaAllocation allocation = nullptr;
	SRDescriptorIndex srvDescriptor = INVALID_DESCRIPTOR_INDEX;
};

struct SRSampler_Vulkan {
	~SRSampler_Vulkan() {
		destructionHandler->enqueue(sampler);
	}

	SRDestructionHandler_Vulkan* destructionHandler = nullptr;
	VkSampler sampler = VK_NULL_HANDLE;
	SRDescriptorIndex samplerDescriptor = INVALID_DESCRIPTOR_INDEX;
};

struct SRCmdList_Vulkan {
	VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
};

struct SRPipeline_Vulkan {
	~SRPipeline_Vulkan() {
		destructionHandler->enqueue(pipeline);
		destructionHandler->enqueue(pipelineLayout);
	}

	SRDestructionHandler_Vulkan* destructionHandler = nullptr;
	VkPipeline pipeline = VK_NULL_HANDLE;
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
};

struct SRSwapchain_Vulkan {
	~SRSwapchain_Vulkan() {
		destructionHandler->enqueue(swapchain);

		for (size_t i = 0; i < imageViews.size(); i++) {
			destructionHandler->enqueue(imageViews[i]);
		}
	}

	SRDestructionHandler_Vulkan* destructionHandler = nullptr;
	VkSwapchainKHR swapchain = VK_NULL_HANDLE;
	VkExtent2D extent = {};
	std::vector<VkImage> images;
	std::vector<VkImageView> imageViews;
};

// ---------------------------- Converter Functions ----------------------------
inline SRBuffer_Vulkan* to_vk_internal(const SRBuffer& buffer) {
	return (SRBuffer_Vulkan*)buffer.internalState.get();
}

inline SRCmdList_Vulkan* to_vk_internal(const SRCmdList& cmdList) {
	return (SRCmdList_Vulkan*)cmdList.internalState;
}

inline SRPipeline_Vulkan* to_vk_internal(const SRPipeline& pipeline) {
	return (SRPipeline_Vulkan*)pipeline.internalState.get();
}

inline SRSwapchain_Vulkan* to_vk_internal(const SRSwapchain& swapchain) {
	return (SRSwapchain_Vulkan*)swapchain.internalState.get();
}

inline SRTexture_Vulkan* to_vk_internal(const SRTexture& texture) {
	return (SRTexture_Vulkan*)texture.internalState.get();
}

inline constexpr VkAttachmentLoadOp to_vk_load_op(SRLoadOp value) {
	switch (value) {
	case SRLoadOp::None:
		return VK_ATTACHMENT_LOAD_OP_NONE; // NOTE: Very new feature, core from Vulkan 1.4
	case SRLoadOp::Load:
		return VK_ATTACHMENT_LOAD_OP_LOAD;
	case SRLoadOp::Clear:
		return VK_ATTACHMENT_LOAD_OP_CLEAR;
	case SRLoadOp::DontCare:
		return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	}
}

inline constexpr VkAttachmentStoreOp to_vk_store_op(SRStoreOp value) {
	switch (value) {
	case SRStoreOp::None:
		return VK_ATTACHMENT_STORE_OP_NONE;
	case SRStoreOp::Store:
		return VK_ATTACHMENT_STORE_OP_STORE;
	case SRStoreOp::DontCare:
		return VK_ATTACHMENT_STORE_OP_DONT_CARE;
	}
}

inline constexpr VkAccessFlags2 to_vk_access_mask(SRAccessMask value) {
	VkAccessFlags2 result = VK_ACCESS_2_NONE;

	if (has_flag(value, SRAccessMask::VertexBuffer)) {
		result |= VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT;
	}
	if (has_flag(value, SRAccessMask::ConstantBuffer)) {
		result |= VK_ACCESS_2_UNIFORM_READ_BIT;
	}
	if (has_flag(value, SRAccessMask::IndexBuffer)) {
		result |= VK_ACCESS_2_INDEX_READ_BIT;
	}
	if (has_flag(value, SRAccessMask::RenderTarget)) {
		result |= VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT; // TODO; Might need VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT too?
	}
	if (has_flag(value, SRAccessMask::UnorderedAccess)) {
		result |= VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
	}
	if (has_flag(value, SRAccessMask::DepthStencilRead)) {
		result |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
	}
	if (has_flag(value, SRAccessMask::DepthStencilWrite)) {
		result |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	}
	if (has_flag(value, SRAccessMask::ShaderResource)) {
		result |= VK_ACCESS_2_SHADER_READ_BIT;
	}
	if (has_flag(value, SRAccessMask::CopySrc)) {
		result |= VK_ACCESS_2_TRANSFER_READ_BIT;
	}
	if (has_flag(value, SRAccessMask::CopyDst)) {
		result |= VK_ACCESS_2_TRANSFER_WRITE_BIT;
	}

	return result;
}

inline constexpr VkImageLayout to_vk_resource_state(SRResourceState value) {
	switch (value) {
	case SRResourceState::Undefined:
		return VK_IMAGE_LAYOUT_UNDEFINED;
	case SRResourceState::RenderTarget:
		return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	case SRResourceState::DepthWrite: // TODO: Might be wrong
		return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	case SRResourceState::DepthRead:
		return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	case SRResourceState::ShaderResource:
		return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	case SRResourceState::UnorderedAccess:
		return VK_IMAGE_LAYOUT_GENERAL;
	default:
		return VK_IMAGE_LAYOUT_GENERAL;
	}
}

inline constexpr VkBorderColor to_vk_sampler_border_color(SRBorderColor value) {
	switch (value) {
	case SRBorderColor::TransparentBlack:
		return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
	case SRBorderColor::OpaqueBlack:
		return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
	case SRBorderColor::OpaqueWhite:
		return VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	default:
		return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
	}
}

inline constexpr VkSamplerAddressMode to_vk_texture_address_mode(SRTextureAddressMode value) {
	switch (value) {
	case SRTextureAddressMode::Wrap:
		return VK_SAMPLER_ADDRESS_MODE_REPEAT;
	case SRTextureAddressMode::Mirror:
		return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
	case SRTextureAddressMode::Clamp:
		return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	case SRTextureAddressMode::Border:
		return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		//case TextureAddressMode::MirrorOnce:
		//	if (features_1_2.samplerMirrorClampToEdge == VK_TRUE) {
		//		return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
		//	}
		//	return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
	default:
		return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	}
}

inline constexpr VkPipelineStageFlags2 to_vk_pipeline_stage(SRPipelineStage value) {
	VkPipelineStageFlags2 result = VK_PIPELINE_STAGE_2_NONE;

	if (has_flag(value, SRPipelineStage::AllCommands)) {
		result |= VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	}
	if (has_flag(value, SRPipelineStage::Draw)) {
		// TODO: LOOK INTO THIS
		// Invalid for now
		assert(false);
	}
	if (has_flag(value, SRPipelineStage::IndexInput)) {
		assert(false);
	}
	if (has_flag(value, SRPipelineStage::VertexShader)) {
		result |= VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
	}
	if (has_flag(value, SRPipelineStage::PixelShader)) {
		result |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	}
	if (has_flag(value, SRPipelineStage::DepthStencil)) {
		// TODO: Investigate
		result |= VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
	}
	if (has_flag(value, SRPipelineStage::RenderTarget)) {
		result |= VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	}
	if (has_flag(value, SRPipelineStage::ComputeShader)) {
		result |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	}
	if (has_flag(value, SRPipelineStage::RayTracing)) {
		assert(false);
	}
	if (has_flag(value, SRPipelineStage::Copy)) {
		result |= VK_PIPELINE_STAGE_2_TRANSFER_BIT;
	}

	return result;
}

inline constexpr VkBlendFactor to_vk_blend(SRBlend value) {
	switch (value) {
	case SRBlend::Zero:
		return VK_BLEND_FACTOR_ZERO;
	case SRBlend::One:
		return VK_BLEND_FACTOR_ONE;
	case SRBlend::SrcColor:
		return VK_BLEND_FACTOR_SRC_COLOR;
	case SRBlend::InvSrcColor:
		return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
	case SRBlend::SrcAlpha:
		return VK_BLEND_FACTOR_SRC_ALPHA;
	case SRBlend::InvSrcAlpha:
		return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	case SRBlend::DstAlpha:
		return VK_BLEND_FACTOR_DST_ALPHA;
	case SRBlend::InvDstAlpha:
		return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
	case SRBlend::DstColor:
		return VK_BLEND_FACTOR_DST_COLOR;
	case SRBlend::InvDstColor:
		return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
	case SRBlend::SrcAlphaSat:
		return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
	case SRBlend::BlendFactor:
		return VK_BLEND_FACTOR_CONSTANT_COLOR;
	case SRBlend::InvBlendFator:
		return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
	case SRBlend::Src1Color:
		return VK_BLEND_FACTOR_SRC1_COLOR;
	case SRBlend::InvSrc1Color:
		return VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
	case SRBlend::Src1Alpha:
		return VK_BLEND_FACTOR_SRC1_ALPHA;
	case SRBlend::InvSrc1Alpha:
		return VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
	default:
		return VK_BLEND_FACTOR_ZERO;
	}
}

inline constexpr VkBlendOp to_vk_blend_op(SRBlendOp value) {
	switch (value) {
	case SRBlendOp::Add:
		return VK_BLEND_OP_ADD;
	case SRBlendOp::Subtract:
		return VK_BLEND_OP_SUBTRACT;
	case SRBlendOp::RevSubtract:
		return VK_BLEND_OP_REVERSE_SUBTRACT;
	case SRBlendOp::Min:
		return VK_BLEND_OP_MIN;
	case SRBlendOp::Max:
		return VK_BLEND_OP_MAX;
	default:
		return VK_BLEND_OP_ADD;
	}
}

inline constexpr VkCompareOp to_vk_comparison_func(SRComparisonFunc value) {
	switch (value) {
	case SRComparisonFunc::Never:
		return VK_COMPARE_OP_NEVER;
	case SRComparisonFunc::Less:
		return VK_COMPARE_OP_LESS;
	case SRComparisonFunc::Equal:
		return VK_COMPARE_OP_EQUAL;
	case SRComparisonFunc::LessEqual:
		return VK_COMPARE_OP_LESS_OR_EQUAL;
	case SRComparisonFunc::Greater:
		return VK_COMPARE_OP_GREATER;
	case SRComparisonFunc::NotEqual:
		return VK_COMPARE_OP_NOT_EQUAL;
	case SRComparisonFunc::GreaterEqual:
		return VK_COMPARE_OP_GREATER_OR_EQUAL;
	case SRComparisonFunc::Always:
		return VK_COMPARE_OP_ALWAYS;
	default:
		return VK_COMPARE_OP_NEVER;
	}
}

inline constexpr VkCullModeFlags to_vk_cull_mode(SRCullMode value) {
	switch (value) {
	case SRCullMode::Front:
		return VK_CULL_MODE_FRONT_BIT;
	case SRCullMode::Back:
		return VK_CULL_MODE_BACK_BIT;
	default:
		return VK_CULL_MODE_NONE;
	}
}

inline constexpr VkFormat to_vk_format(SRFormat format) {
	switch (format) {
	case SRFormat::Unknown:
		return VK_FORMAT_UNDEFINED;
	case SRFormat::RGBA32_FLOAT:
		return VK_FORMAT_R32G32B32A32_SFLOAT;
	case SRFormat::RGBA32_UINT:
		return VK_FORMAT_R32G32B32A32_UINT;
	case SRFormat::RGBA32_SINT:
		return VK_FORMAT_R32G32B32A32_SINT;
	case SRFormat::RGB32_FLOAT:
		return VK_FORMAT_R32G32B32_SFLOAT;
	case SRFormat::RGB32_UINT:
		return VK_FORMAT_R32G32B32_UINT;
	case SRFormat::RGB32_SINT:
		return VK_FORMAT_R32G32B32_SINT;
	case SRFormat::RGBA16_FLOAT:
		return VK_FORMAT_R16G16B16A16_SFLOAT;
	case SRFormat::RGBA16_UNORM:
		return VK_FORMAT_R16G16B16A16_UNORM;
	case SRFormat::RGBA16_UINT:
		return VK_FORMAT_R16G16B16A16_UINT;
	case SRFormat::RGBA16_SNORM:
		return VK_FORMAT_R16G16B16A16_SNORM;
	case SRFormat::RGBA16_SINT:
		return VK_FORMAT_R16G16B16A16_SINT;
	case SRFormat::RG32_FLOAT:
		return VK_FORMAT_R32G32_SFLOAT;
	case SRFormat::RG32_UINT:
		return VK_FORMAT_R32G32_UINT;
	case SRFormat::RG32_SINT:
		return VK_FORMAT_R32G32_SINT;
	case SRFormat::D32_FLOAT_S8X24_UINT:
		return VK_FORMAT_D32_SFLOAT_S8_UINT;
	case SRFormat::RGB10A2_UNORM:
		return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
	case SRFormat::RGB10A2_UINT:
		return VK_FORMAT_A2B10G10R10_UINT_PACK32;
	case SRFormat::RG11B10_FLOAT:
		return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
	case SRFormat::RGBA8_UNORM:
		return VK_FORMAT_R8G8B8A8_UNORM;
	case SRFormat::RGBA8_UNORM_SRGB:
		return VK_FORMAT_R8G8B8A8_SRGB;
	case SRFormat::RGBA8_UINT:
		return VK_FORMAT_R8G8B8A8_UINT;
	case SRFormat::RGBA8_SNORM:
		return VK_FORMAT_R8G8B8A8_SNORM;
	case SRFormat::RGBA8_SINT:
		return VK_FORMAT_R8G8B8A8_SINT;
	case SRFormat::RG16_FLOAT:
		return VK_FORMAT_R16G16_SFLOAT;
	case SRFormat::RG16_UNORM:
		return VK_FORMAT_R16G16_UNORM;
	case SRFormat::RG16_UINT:
		return VK_FORMAT_R16G16_UINT;
	case SRFormat::RG16_SNORM:
		return VK_FORMAT_R16G16_SNORM;
	case SRFormat::RG16_SINT:
		return VK_FORMAT_R16G16_SINT;
	case SRFormat::D32_FLOAT:
		return VK_FORMAT_D32_SFLOAT;
	case SRFormat::R32_FLOAT:
		return VK_FORMAT_R32_SFLOAT;
	case SRFormat::R32_UINT:
		return VK_FORMAT_R32_UINT;
	case SRFormat::R32_SINT:
		return VK_FORMAT_R32_SINT;
	case SRFormat::D24_UNORM_S8_UINT:
		return VK_FORMAT_D24_UNORM_S8_UINT;
	case SRFormat::RGB9E5_SHAREDEXP:
		return VK_FORMAT_E5B9G9R9_UFLOAT_PACK32;
	case SRFormat::RG8_UNORM:
		return VK_FORMAT_R8G8_UNORM;
	case SRFormat::RG8_UINT:
		return VK_FORMAT_R8G8_UINT;
	case SRFormat::RG8_SNORM:
		return VK_FORMAT_R8G8_SNORM;
	case SRFormat::RG8_SINT:
		return VK_FORMAT_R8G8_SINT;
	case SRFormat::R16_FLOAT:
		return VK_FORMAT_R16_SFLOAT;
	case SRFormat::D16_UNORM:
		return VK_FORMAT_D16_UNORM;
	case SRFormat::R16_UNORM:
		return VK_FORMAT_R16_UNORM;
	case SRFormat::R16_UINT:
		return VK_FORMAT_R16_UINT;
	case SRFormat::R16_SNORM:
		return VK_FORMAT_R16_SNORM;
	case SRFormat::R16_SINT:
		return VK_FORMAT_R16_SINT;
	case SRFormat::R8_UNORM:
		return VK_FORMAT_R8_UNORM;
	case SRFormat::R8_UINT:
		return VK_FORMAT_R8_UINT;
	case SRFormat::R8_SNORM:
		return VK_FORMAT_R8_SNORM;
	case SRFormat::R8_SINT:
		return VK_FORMAT_R8_SINT;
	case SRFormat::BC1_UNORM:
		return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
	case SRFormat::BC1_UNORM_SRGB:
		return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
	case SRFormat::BC2_UNORM:
		return VK_FORMAT_BC2_UNORM_BLOCK;
	case SRFormat::BC2_UNORM_SRGB:
		return VK_FORMAT_BC2_SRGB_BLOCK;
	case SRFormat::BC3_UNORM:
		return VK_FORMAT_BC3_UNORM_BLOCK;
	case SRFormat::BC3_UNORM_SRGB:
		return VK_FORMAT_BC3_SRGB_BLOCK;
	case SRFormat::BC4_UNORM:
		return VK_FORMAT_BC4_UNORM_BLOCK;
	case SRFormat::BC4_SNORM:
		return VK_FORMAT_BC4_SNORM_BLOCK;
	case SRFormat::BC5_UNORM:
		return VK_FORMAT_BC5_UNORM_BLOCK;
	case SRFormat::BC5_SNORM:
		return VK_FORMAT_BC5_SNORM_BLOCK;
	case SRFormat::BGRA8_UNORM:
		return VK_FORMAT_B8G8R8A8_UNORM;
	case SRFormat::BGRA8_UNORM_SRGB:
		return VK_FORMAT_B8G8R8A8_SRGB;
	case SRFormat::BC6H_UF16:
		return VK_FORMAT_BC6H_UFLOAT_BLOCK;
	case SRFormat::BC6H_SF16:
		return VK_FORMAT_BC6H_SFLOAT_BLOCK;
	case SRFormat::BC7_UNORM:
		return VK_FORMAT_BC7_UNORM_BLOCK;
	case SRFormat::BC7_UNORM_SRGB:
		return VK_FORMAT_BC7_SRGB_BLOCK;
	case SRFormat::NV12:
		return VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
	default:
		return VK_FORMAT_UNDEFINED;
	}
}