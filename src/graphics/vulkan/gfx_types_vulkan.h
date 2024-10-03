#pragma once

#include "../gfx_types.h"
#include <vulkan/vulkan.h>

#include <deque>

struct DestructionHandler {
	~DestructionHandler() {
		update(~0, 0);

		vkDestroyDevice(device, nullptr);
		vkDestroyInstance(instance, nullptr);
	}

	VkDevice device = nullptr;
	VkInstance instance = nullptr;
	uint64_t frameCount = 0;

	std::deque<std::pair<VkCommandPool, uint64_t>> commandPools = {};
	std::deque<std::pair<VkDescriptorPool, uint64_t>> descriptorPools = {};
	std::deque<std::pair<VkDescriptorSetLayout, uint64_t>> descriptorSetLayouts = {};
	std::deque<std::pair<VkFence, uint64_t>> fences = {};
	std::deque<std::pair<VkImageView, uint64_t>> imageViews = {};
	std::deque<std::pair<VkPipeline, uint64_t>> pipelines = {};
	std::deque<std::pair<VkPipelineLayout, uint64_t>> pipelineLayouts = {};
	std::deque<std::pair<VkSampler, uint64_t>> samplers = {};
	std::deque<std::pair<VkSemaphore, uint64_t>> semaphores = {};
	std::deque<std::pair<VkShaderModule, uint64_t>> shaderModules = {};
	std::deque<std::pair<VkSurfaceKHR, uint64_t>> surfaces = {};
	std::deque<std::pair<VkSwapchainKHR, uint64_t>> swapchains = {};
	std::deque<std::pair<VkBuffer, uint64_t>> buffers = {};
	std::deque<std::pair<VkImage, uint64_t>> images = {};
	std::deque<std::pair<VkDeviceMemory, uint64_t>> allocations = {};

	// Called once per frame and destroys objects automatically
	void update(uint64_t frameCount, uint32_t bufferCount) {
		const auto destroy = [&](auto&& queue, auto&& handler) {
			while (!queue.empty()) {
				if (queue.front().second + bufferCount < frameCount) {
					auto item = queue.front();
					queue.pop_front();
					handler(item.first);
				}
				else {
					break;
				}
			}
			};

		destroy(semaphores, [&](auto& item) {
			vkDestroySemaphore(device, item, nullptr);
			});

		destroy(fences, [&](auto& item) {
			vkDestroyFence(device, item, nullptr);
			});

		destroy(commandPools, [&](auto& item) {
			vkDestroyCommandPool(device, item, nullptr);
			});

		destroy(images, [&](auto& item) {
			vkDestroyImage(device, item, nullptr);
			});

		destroy(imageViews, [&](auto& item) {
			vkDestroyImageView(device, item, nullptr);
			});

		destroy(buffers, [&](auto& item) {
			vkDestroyBuffer(device, item, nullptr);
			});

		destroy(allocations, [&](auto& item) {
			vkFreeMemory(device, item, nullptr);
			});

		destroy(samplers, [&](auto& item) {
			vkDestroySampler(device, item, nullptr);
			});

		destroy(descriptorPools, [&](auto& item) {
			vkDestroyDescriptorPool(device, item, nullptr);
			});

		destroy(descriptorSetLayouts, [&](auto& item) {
			vkDestroyDescriptorSetLayout(device, item, nullptr);
			});

		destroy(shaderModules, [&](auto& item) {
			vkDestroyShaderModule(device, item, nullptr);
			});

		destroy(pipelines, [&](auto& item) {
			vkDestroyPipeline(device, item, nullptr);
			});

		destroy(pipelineLayouts, [&](auto& item) {
			vkDestroyPipelineLayout(device, item, nullptr);
			});

		destroy(swapchains, [&](auto& item) {
			vkDestroySwapchainKHR(device, item, nullptr);
			});

		destroy(surfaces, [&](auto& item) {
			vkDestroySurfaceKHR(instance, item, nullptr);
			});

		this->frameCount = frameCount;
	}
};

struct CommandList_Vulkan {
	VkCommandBuffer commandBuffers[GFXDevice::FRAMES_IN_FLIGHT] = { nullptr };
};

struct Sampler_Vulkan {
	~Sampler_Vulkan() {
		const uint64_t frameCount = destructionHandler->frameCount;
		destructionHandler->samplers.push_back({ sampler, frameCount });
	}

	DestructionHandler* destructionHandler = nullptr;
	VkSampler sampler = nullptr;
};

struct SwapChain_Vulkan {
	~SwapChain_Vulkan() {
		const uint64_t frameCount = destructionHandler->frameCount;
		destructionHandler->swapchains.push_back({ swapChain, frameCount });

		for (size_t i = 0; i < images.size(); i++) {
			destructionHandler->imageViews.push_back({ imageViews[i], frameCount });
		}
	}

	DestructionHandler* destructionHandler = nullptr;
	SwapChainInfo info = {};
	VkSwapchainKHR swapChain = nullptr;
	VkSurfaceKHR surface = nullptr;
	VkFormat format = VK_FORMAT_UNDEFINED;
	VkExtent2D extent = {};

	std::vector<VkImage> images = {};
	std::vector<VkImageView> imageViews = {};
};

struct Shader_Vulkan {
	~Shader_Vulkan() {
		const uint64_t frameCount = destructionHandler->frameCount;
		destructionHandler->shaderModules.push_back({ shaderModule, frameCount });
	}

	DestructionHandler* destructionHandler = nullptr;
	VkShaderModule shaderModule = nullptr;
	std::vector<uint8_t> shaderCode = {};
};

struct Pipeline_Vulkan {
	~Pipeline_Vulkan() {
		const uint64_t frameCount = destructionHandler->frameCount;
		destructionHandler->pipelines.push_back({ pipeline, frameCount });
		destructionHandler->pipelineLayouts.push_back({ pipelineLayout, frameCount });
	}

	DestructionHandler* destructionHandler = nullptr;
	PipelineInfo info = {};
	VkPipeline pipeline = nullptr;
	VkPipelineLayout pipelineLayout = nullptr;
};

// Vulkan converter functions
CommandList_Vulkan* to_internal(const CommandList& cmdList) {
	return (CommandList_Vulkan*)cmdList.internalState;
}

Shader_Vulkan* to_internal(const Shader& shader) {
	return (Shader_Vulkan*)shader.internalState.get();
}

SwapChain_Vulkan* to_internal(const SwapChain& swapChain) {
	return (SwapChain_Vulkan*)swapChain.internalState.get();
}

Pipeline_Vulkan* to_internal(const Pipeline& pipeline) {
	return (Pipeline_Vulkan*)pipeline.internalState.get();
}

constexpr VkCompareOp to_vk_comparison_func(ComparisonFunc value) {
	switch (value) {
	case ComparisonFunc::NEVER:
		return VK_COMPARE_OP_NEVER;
	case ComparisonFunc::LESS:
		return VK_COMPARE_OP_LESS;
	case ComparisonFunc::EQUAL:
		return VK_COMPARE_OP_EQUAL;
	case ComparisonFunc::LESS_EQUAL:
		return VK_COMPARE_OP_LESS_OR_EQUAL;
	case ComparisonFunc::GREATER:
		return VK_COMPARE_OP_GREATER;
	case ComparisonFunc::NOT_EQUAL:
		return VK_COMPARE_OP_NOT_EQUAL;
	case ComparisonFunc::GREATER_EQUAL:
		return VK_COMPARE_OP_GREATER_OR_EQUAL;
	case ComparisonFunc::ALWAYS:
		return VK_COMPARE_OP_ALWAYS;
	default:
		return VK_COMPARE_OP_NEVER;
	}
}

constexpr VkCullModeFlags to_vk_cull_mode(CullMode value) {
	switch (value) {
	case CullMode::FRONT:
		return VK_CULL_MODE_FRONT_BIT;
	case CullMode::BACK:
		return VK_CULL_MODE_BACK_BIT;
	default:
		return VK_CULL_MODE_NONE;
	}
}

constexpr VkFormat to_vk_format(Format value) {
	switch (value) {
	case Format::UNKNOWN:
		return VK_FORMAT_UNDEFINED;
	case Format::R32G32B32A32_FLOAT:
		return VK_FORMAT_R32G32B32A32_SFLOAT;
	case Format::R32G32B32A32_UINT:
		return VK_FORMAT_R32G32B32A32_UINT;
	case Format::R32G32B32A32_SINT:
		return VK_FORMAT_R32G32B32A32_SINT;
	case Format::R32G32B32_FLOAT:
		return VK_FORMAT_R32G32B32_SFLOAT;
	case Format::R32G32B32_UINT:
		return VK_FORMAT_R32G32B32_UINT;
	case Format::R32G32B32_SINT:
		return VK_FORMAT_R32G32B32_SINT;
	case Format::R16G16B16A16_FLOAT:
		return VK_FORMAT_R16G16B16A16_SFLOAT;
	case Format::R16G16B16A16_UNORM:
		return VK_FORMAT_R16G16B16A16_UNORM;
	case Format::R16G16B16A16_UINT:
		return VK_FORMAT_R16G16B16A16_UINT;
	case Format::R16G16B16A16_SNORM:
		return VK_FORMAT_R16G16B16A16_SNORM;
	case Format::R16G16B16A16_SINT:
		return VK_FORMAT_R16G16B16A16_SINT;
	case Format::R32G32_FLOAT:
		return VK_FORMAT_R32G32_SFLOAT;
	case Format::R32G32_UINT:
		return VK_FORMAT_R32G32_UINT;
	case Format::R32G32_SINT:
		return VK_FORMAT_R32G32_SINT;
	case Format::D32_FLOAT_S8X24_UINT:
		return VK_FORMAT_D32_SFLOAT_S8_UINT;
	case Format::R10G10B10A2_UNORM:
		return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
	case Format::R10G10B10A2_UINT:
		return VK_FORMAT_A2B10G10R10_UINT_PACK32;
	case Format::R11G11B10_FLOAT:
		return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
	case Format::R8G8B8A8_UNORM:
		return VK_FORMAT_R8G8B8A8_UNORM;
	case Format::R8G8B8A8_UNORM_SRGB:
		return VK_FORMAT_R8G8B8A8_SRGB;
	case Format::R8G8B8A8_UINT:
		return VK_FORMAT_R8G8B8A8_UINT;
	case Format::R8G8B8A8_SNORM:
		return VK_FORMAT_R8G8B8A8_SNORM;
	case Format::R8G8B8A8_SINT:
		return VK_FORMAT_R8G8B8A8_SINT;
	case Format::R16G16_FLOAT:
		return VK_FORMAT_R16G16_SFLOAT;
	case Format::R16G16_UNORM:
		return VK_FORMAT_R16G16_UNORM;
	case Format::R16G16_UINT:
		return VK_FORMAT_R16G16_UINT;
	case Format::R16G16_SNORM:
		return VK_FORMAT_R16G16_SNORM;
	case Format::R16G16_SINT:
		return VK_FORMAT_R16G16_SINT;
	case Format::D32_FLOAT:
		return VK_FORMAT_D32_SFLOAT;
	case Format::R32_FLOAT:
		return VK_FORMAT_R32_SFLOAT;
	case Format::R32_UINT:
		return VK_FORMAT_R32_UINT;
	case Format::R32_SINT:
		return VK_FORMAT_R32_SINT;
	case Format::D24_UNORM_S8_UINT:
		return VK_FORMAT_D24_UNORM_S8_UINT;
	case Format::R9G9B9E5_SHAREDEXP:
		return VK_FORMAT_E5B9G9R9_UFLOAT_PACK32;
	case Format::R8G8_UNORM:
		return VK_FORMAT_R8G8_UNORM;
	case Format::R8G8_UINT:
		return VK_FORMAT_R8G8_UINT;
	case Format::R8G8_SNORM:
		return VK_FORMAT_R8G8_SNORM;
	case Format::R8G8_SINT:
		return VK_FORMAT_R8G8_SINT;
	case Format::R16_FLOAT:
		return VK_FORMAT_R16_SFLOAT;
	case Format::D16_UNORM:
		return VK_FORMAT_D16_UNORM;
	case Format::R16_UNORM:
		return VK_FORMAT_R16_UNORM;
	case Format::R16_UINT:
		return VK_FORMAT_R16_UINT;
	case Format::R16_SNORM:
		return VK_FORMAT_R16_SNORM;
	case Format::R16_SINT:
		return VK_FORMAT_R16_SINT;
	case Format::R8_UNORM:
		return VK_FORMAT_R8_UNORM;
	case Format::R8_UINT:
		return VK_FORMAT_R8_UINT;
	case Format::R8_SNORM:
		return VK_FORMAT_R8_SNORM;
	case Format::R8_SINT:
		return VK_FORMAT_R8_SINT;
	case Format::BC1_UNORM:
		return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
	case Format::BC1_UNORM_SRGB:
		return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
	case Format::BC2_UNORM:
		return VK_FORMAT_BC2_UNORM_BLOCK;
	case Format::BC2_UNORM_SRGB:
		return VK_FORMAT_BC2_SRGB_BLOCK;
	case Format::BC3_UNORM:
		return VK_FORMAT_BC3_UNORM_BLOCK;
	case Format::BC3_UNORM_SRGB:
		return VK_FORMAT_BC3_SRGB_BLOCK;
	case Format::BC4_UNORM:
		return VK_FORMAT_BC4_UNORM_BLOCK;
	case Format::BC4_SNORM:
		return VK_FORMAT_BC4_SNORM_BLOCK;
	case Format::BC5_UNORM:
		return VK_FORMAT_BC5_UNORM_BLOCK;
	case Format::BC5_SNORM:
		return VK_FORMAT_BC5_SNORM_BLOCK;
	case Format::B8G8R8A8_UNORM:
		return VK_FORMAT_B8G8R8A8_UNORM;
	case Format::B8G8R8A8_UNORM_SRGB:
		return VK_FORMAT_B8G8R8A8_SRGB;
	case Format::BC6H_UF16:
		return VK_FORMAT_BC6H_UFLOAT_BLOCK;
	case Format::BC6H_SF16:
		return VK_FORMAT_BC6H_SFLOAT_BLOCK;
	case Format::BC7_UNORM:
		return VK_FORMAT_BC7_UNORM_BLOCK;
	case Format::BC7_UNORM_SRGB:
		return VK_FORMAT_BC7_SRGB_BLOCK;
	case Format::NV12:
		return VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
	}

	return VK_FORMAT_UNDEFINED;
}

constexpr VkPipelineStageFlags2 to_vk_pipeline_stage(ResourceState value) {
	VkPipelineStageFlags2 flags = VK_PIPELINE_STAGE_2_NONE;

	if (has_flag(value, ResourceState::SHADER_RESOURCE) ||
		has_flag(value, ResourceState::UNORDERED_ACCESS)) {
		flags |= VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	}
	if (has_flag(value, ResourceState::COPY_SRC) ||
		has_flag(value, ResourceState::COPY_DST)) {
		flags |= VK_PIPELINE_STAGE_2_TRANSFER_BIT;
	}
	if (has_flag(value, ResourceState::RENDER_TARGET)) {
		flags |= VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	}
	if (has_flag(value, ResourceState::DEPTH_READ) ||
		has_flag(value, ResourceState::DEPTH_WRITE)) {
		flags |= VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
	}

	return flags;
}

constexpr VkAccessFlags2 to_vk_resource_access(ResourceState value) {
	VkAccessFlags2 flags = 0;

	if (has_flag(value, ResourceState::SHADER_RESOURCE)) {
		flags |= VK_ACCESS_2_SHADER_READ_BIT;
	}
	if (has_flag(value, ResourceState::UNORDERED_ACCESS)) {
		flags |= VK_ACCESS_2_SHADER_READ_BIT;
		flags |= VK_ACCESS_2_SHADER_WRITE_BIT;
	}
	if (has_flag(value, ResourceState::COPY_SRC)) {
		flags |= VK_ACCESS_2_TRANSFER_READ_BIT;
	}
	if (has_flag(value, ResourceState::COPY_DST)) {
		flags |= VK_ACCESS_2_TRANSFER_WRITE_BIT;
	}
	if (has_flag(value, ResourceState::RENDER_TARGET)) {
		flags |= VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;
		flags |= VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	}
	if (has_flag(value, ResourceState::DEPTH_READ)) {
		flags |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
	}
	if (has_flag(value, ResourceState::DEPTH_WRITE)) {
		flags |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	}

	return flags;
}

constexpr VkImageLayout to_vk_resource_state(ResourceState value) {
	switch (value) {
	case ResourceState::UNDEFINED:
		return VK_IMAGE_LAYOUT_UNDEFINED;
	case ResourceState::RENDER_TARGET:
		return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	case ResourceState::DEPTH_WRITE: // TODO: Might be wrong
		return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	case ResourceState::DEPTH_READ:
		return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	case ResourceState::SHADER_RESOURCE:
		return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	case ResourceState::UNORDERED_ACCESS:
		return VK_IMAGE_LAYOUT_GENERAL;
	default:
		return VK_IMAGE_LAYOUT_GENERAL;
	}
}

constexpr VkBorderColor to_vk_sampler_border_color(BorderColor value) {
	switch (value) {
	case BorderColor::TRANSPARENT_BLACK:
		return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
	case BorderColor::OPAQUE_BLACK:
		return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
	case BorderColor::OPAQUE_WHITE:
		return VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	default:
		return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
	}
}

constexpr VkSamplerAddressMode to_vk_texture_address_mode(TextureAddressMode value) {
	switch (value) {
	case TextureAddressMode::WRAP:
		return VK_SAMPLER_ADDRESS_MODE_REPEAT;
	case TextureAddressMode::MIRROR:
		return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
	case TextureAddressMode::CLAMP:
		return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	case TextureAddressMode::BORDER:
		return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		//case TextureAddressMode::MIRROR_ONCE:
		//	if (features_1_2.samplerMirrorClampToEdge == VK_TRUE) {
		//		return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
		//	}
		//	return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
	default:
		return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	}
}

namespace vk_helpers {
	struct ImageTransitionInfo {
		VkImage image = nullptr;
		VkImageLayout oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		VkImageLayout newLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		VkAccessFlags2 srcAccessMask = VK_ACCESS_2_NONE;
		VkAccessFlags2 dstAccessMask = VK_ACCESS_2_NONE;
		VkPipelineStageFlags2 srcStageMask = VK_PIPELINE_STAGE_2_NONE;
		VkPipelineStageFlags2 dstStageMask = VK_PIPELINE_STAGE_2_NONE;
		VkImageAspectFlags aspectFlags = 0;
	};

	void transition_image_layout(const ImageTransitionInfo& info, VkCommandBuffer commandBuffer) {
		// TODO: Doesn't work for depth attachments
		const VkImageSubresourceRange subresourceRange = {
			.aspectMask = info.aspectFlags,
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