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