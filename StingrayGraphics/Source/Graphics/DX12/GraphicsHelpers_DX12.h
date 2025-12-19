#pragma once

#include "Graphics/DX12/GraphicsTypes_DX12.h"

struct SRDeviceCapabilities_DX12 {
	D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_12_0;
	D3D12_RESOURCE_BINDING_TIER bindingTier = D3D12_RESOURCE_BINDING_TIER_1;
	D3D12_RESOURCE_HEAP_TIER heapTier = D3D12_RESOURCE_HEAP_TIER_2;
	D3D12_TILED_RESOURCES_TIER tiledTier = D3D12_TILED_RESOURCES_TIER_NOT_SUPPORTED;
	D3D12_CONSERVATIVE_RASTERIZATION_TIER conservRasterTier = D3D12_CONSERVATIVE_RASTERIZATION_TIER_NOT_SUPPORTED;
	D3D12_RAYTRACING_TIER rayTracingTier = D3D12_RAYTRACING_TIER_NOT_SUPPORTED;
	D3D12_MESH_SHADER_TIER meshShaderTier = D3D12_MESH_SHADER_TIER_NOT_SUPPORTED;
	D3D_SHADER_MODEL shaderModel = D3D_SHADER_MODEL_NONE;
	bool enhancedBarriersSupported = false;
};

struct SRImageTransitionInfo_DX12 {
	ID3D12Resource* image;
	D3D12_BARRIER_LAYOUT  oldLayout = D3D12_BARRIER_LAYOUT_UNDEFINED;
	D3D12_BARRIER_LAYOUT  newLayout = D3D12_BARRIER_LAYOUT_UNDEFINED;
	D3D12_BARRIER_ACCESS srcAccessMask = D3D12_BARRIER_ACCESS_NO_ACCESS;
	D3D12_BARRIER_ACCESS dstAccessMask = D3D12_BARRIER_ACCESS_NO_ACCESS;
	D3D12_BARRIER_SYNC srcStageMask = D3D12_BARRIER_SYNC_NONE;
	D3D12_BARRIER_SYNC dstStageMask = D3D12_BARRIER_SYNC_NONE;
	D3D12_TEXTURE_BARRIER_FLAGS flags = D3D12_TEXTURE_BARRIER_FLAG_NONE;
};

namespace SRDX12Helpers {
	inline SRDeviceCapabilities_DX12 query_device_capabilities(ID3D12Device* device) {
		const D3D_FEATURE_LEVEL levels[] = {
			D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0
		};

		D3D12_FEATURE_DATA_FEATURE_LEVELS featureLevels = {
			.NumFeatureLevels = std::size(levels),
			.pFeatureLevelsRequested = levels
		};
		D3D12_FEATURE_DATA_D3D12_OPTIONS options = {};
		D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
		D3D12_FEATURE_DATA_D3D12_OPTIONS7 options7 = {};
		D3D12_FEATURE_DATA_D3D12_OPTIONS12 options12 = {};
		D3D12_FEATURE_DATA_SHADER_MODEL shaderModel = {};

		SR_DX12_CHECK(device->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &featureLevels, sizeof(featureLevels)), "Check feature level");
		SR_DX12_CHECK(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &options, sizeof(options)), "Check feature options");
		SR_DX12_CHECK(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5)), "Check feature options5");
		SR_DX12_CHECK(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7, &options7, sizeof(options7)), "Check feature options7");
		SR_DX12_CHECK(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS12, &options12, sizeof(options12)), "Check feature options12");

		// Find the highest supported shader model
		D3D_SHADER_MODEL sm = D3D_HIGHEST_SHADER_MODEL;
		while (sm >= D3D_SHADER_MODEL_6_0) {
			shaderModel.HighestShaderModel = sm;
			if (SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &shaderModel, sizeof(shaderModel)))) {
				break;
			}
			sm = static_cast<D3D_SHADER_MODEL>(static_cast<int>(sm) - 1);
		}

		const SRDeviceCapabilities_DX12 deviceCapabilities = {
			.featureLevel = featureLevels.MaxSupportedFeatureLevel,
			.bindingTier = options.ResourceBindingTier,
			.heapTier = options.ResourceHeapTier,
			.tiledTier = options.TiledResourcesTier,
			.conservRasterTier = options.ConservativeRasterizationTier,
			.rayTracingTier = options5.RaytracingTier,
			.meshShaderTier = options7.MeshShaderTier,
			.shaderModel = shaderModel.HighestShaderModel,
			.enhancedBarriersSupported = options12.EnhancedBarriersSupported == TRUE
		};
		return deviceCapabilities;
	}

	inline void transition_image_layout(const SRImageTransitionInfo_DX12& info, ID3D12GraphicsCommandList7* graphicsCmdList) {
		const D3D12_TEXTURE_BARRIER textureBarrier = {
			.SyncBefore = info.srcStageMask,
			.SyncAfter = info.dstStageMask,
			.AccessBefore = info.srcAccessMask,
			.AccessAfter = info.dstAccessMask,
			.LayoutBefore = info.oldLayout,
			.LayoutAfter = info.newLayout,
			.pResource = info.image,
			.Subresources = { 0xffffffff, 0, 0, 0, 0, 0 },
			.Flags = info.flags
		};

		const D3D12_BARRIER_GROUP barrierGroup = {
			.Type = D3D12_BARRIER_TYPE_TEXTURE,
			.NumBarriers = 1,
			.pTextureBarriers = &textureBarrier
		};

		graphicsCmdList->Barrier(1, &barrierGroup);
	}

	inline SRDescriptorIndex init_rtv_descriptor(ID3D12Device* device, ID3D12Resource* res, const D3D12_RENDER_TARGET_VIEW_DESC& desc, SRDescriptorHeap_DX12& descriptorHeap) {
		const SRDescriptorIndex descriptor = descriptorHeap.get_next_index();

		device->CreateRenderTargetView(
			res,
			&desc,
			descriptorHeap.get_cpu_handle(descriptor)
		);

		return descriptor;
	}

	inline SRDescriptorIndex init_dsv_descriptor(ID3D12Device* device, ID3D12Resource* res, const D3D12_DEPTH_STENCIL_VIEW_DESC& desc, SRDescriptorHeap_DX12& descriptorHeap) {
		const SRDescriptorIndex descriptor = descriptorHeap.get_next_index();

		device->CreateDepthStencilView(
			res,
			&desc,
			descriptorHeap.get_cpu_handle(descriptor)
		);

		return descriptor;
	}

	inline SRDescriptorIndex init_srv_descriptor(ID3D12Device* device, ID3D12Resource* res, const D3D12_SHADER_RESOURCE_VIEW_DESC& desc, SRDescriptorHeap_DX12& descriptorHeap) {
		const SRDescriptorIndex descriptor = descriptorHeap.get_next_index();

		device->CreateShaderResourceView(
			res,
			&desc,
			descriptorHeap.get_cpu_handle(descriptor)
		);

		return descriptor;
	}
}