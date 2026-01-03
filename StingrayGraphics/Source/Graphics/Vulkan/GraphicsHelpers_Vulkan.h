#pragma once

#include "Core/Logger.h"
#include "Graphics/Vulkan/GraphicsTypes_Vulkan.h"

#include <stdexcept>
#include <volk.h>
#include <vector>

struct SRImageTransitionInfo {
	VkImage image = nullptr;
	VkImageLayout oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VkImageLayout newLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VkAccessFlags2 srcAccessMask = VK_ACCESS_2_NONE;
	VkAccessFlags2 dstAccessMask = VK_ACCESS_2_NONE;
	VkPipelineStageFlags2 srcStageMask = VK_PIPELINE_STAGE_2_NONE;
	VkPipelineStageFlags2 dstStageMask = VK_PIPELINE_STAGE_2_NONE;
	VkImageAspectFlags aspectFlags = 0;
};

struct SRSwapchainSupportInfo {
	VkSurfaceCapabilitiesKHR capabilities = {};
	std::vector<VkSurfaceFormatKHR> surfaceFormats;
	std::vector<VkPresentModeKHR> presentModes;
};

namespace SRVulkanHelpers {
	// NOTE: If a format has more than 8 bits per channel, we classify it as HDR
	inline constexpr bool is_hdr_format(VkFormat format) {
		switch (format) {
		case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
		case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
		case VK_FORMAT_R16G16B16A16_SFLOAT:
		case VK_FORMAT_R16G16B16A16_UNORM:
		case VK_FORMAT_R16G16B16A16_SNORM:
			return true;
		default:
			return false;
		}
	}

	inline constexpr bool is_hdr_colorspace(VkColorSpaceKHR colorspace) {
		switch (colorspace) {
		case VK_COLOR_SPACE_HDR10_ST2084_EXT:
		case VK_COLOR_SPACE_HDR10_HLG_EXT:
			return true;
		default:
			return false;
		}
	}

	inline VkSurfaceFormatKHR pick_surface_format(
		VkFormat requestedFormat,
		bool preferHDR,
		const std::vector<VkSurfaceFormatKHR>& available
	) {
		VkSurfaceFormatKHR best = { VK_FORMAT_UNDEFINED, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };

		for (const auto& sf : available) {
			if (sf.format != requestedFormat) {
				continue;
			}

			if (preferHDR && is_hdr_colorspace(sf.colorSpace)) {
				return sf;
			}

			if (sf.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
				best = sf;
			}
		}

		if (best.format == VK_FORMAT_UNDEFINED) {
			SRLOG_CRITICAL_CAT(SRLOG_CAT_VULKAN, "Requested swapchain format not supported");
			throw std::runtime_error("VULKAN ERROR: Requested swapchain format not supported");
		}

		if (preferHDR && (!is_hdr_format(best.format) || !is_hdr_colorspace(best.colorSpace))) {
			SRLOG_WARN_CAT(SRLOG_CAT_VULKAN, "HDR mode requested, but no HDR-capable surface format/colorspace available.");
		}

		return best;
	}

	inline VkPresentModeKHR pick_present_mode(
		bool vSync,
		const std::vector<VkPresentModeKHR>& available
	) {
		// NOTE: For now we simply disallow mailbox presentation all together,
		// since the setting "VSync OFF" indeed means IMMEDIATE presentation
		// since we don't cap to vertical blanks. Whereas mailbox DOES await
		// vertical blank but the framerate is uncapped. I.e. it's either
		// FIFO or IMMEDIATE

		if (!vSync) {
			for (const auto& presentMode : available) {
				if (presentMode == VK_PRESENT_MODE_IMMEDIATE_KHR && !vSync) {
					return VK_PRESENT_MODE_IMMEDIATE_KHR;
				}
			}
		
			SRLOG_CRITICAL_CAT(SRLOG_CAT_VULKAN, "VK_PRESENT_MODE_IMMEDIATE_KHR not supported");
		}

		return VK_PRESENT_MODE_FIFO_KHR;
	}

	inline SRSwapchainSupportInfo query_swapchain_support(VkPhysicalDevice device, VkSurfaceKHR surface) {
		SRSwapchainSupportInfo info = {};

		// Capabilities
		SR_VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &info.capabilities), "Query surface capabilities");

		// Formats
		u32 formatCount;
		SR_VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr), "Query surface formats");

		if (formatCount != 0) {
			info.surfaceFormats.resize(formatCount);
			SR_VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, info.surfaceFormats.data()), "Query surface formats");
		}

		// Present modes
		u32 presentModeCount;
		SR_VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr), "Query surface present modes");

		if (presentModeCount != 0) {
			info.presentModes.resize(presentModeCount);
			SR_VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, info.presentModes.data()), "Query surface present modes");
		}

		return info;
	}

	inline void transition_image_layout(const SRImageTransitionInfo& info, VkCommandBuffer cmdBuffer) {
		// TODO: Doesn't work for depth attachments nor multiple mips
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

		vkCmdPipelineBarrier2(cmdBuffer, &dependencyInfo);
	}

	inline VkShaderModule create_shader_module(VkDevice device, const SRShader* shader) {
		if (!shader || !shader->data) {
			return VK_NULL_HANDLE;
		}

		const VkShaderModuleCreateInfo shaderModuleInfo = {
			.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
			.codeSize = shader->size,
			.pCode = (u32*)shader->data
		};

		VkShaderModule shaderModule;
		SR_VK_CHECK(vkCreateShaderModule(device, &shaderModuleInfo, nullptr, &shaderModule), "Create shader module");
		return shaderModule;
	}
}
