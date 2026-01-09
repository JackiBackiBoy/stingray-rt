#include "GraphicsDevice_DX12.h"
#include "Graphics/DX12/GraphicsHelpers_DX12.h"
#include "Graphics/DX12/GraphicsTypes_DX12.h"
#include "Core/Logger.h"
#include "Core/StringTypes.h"
#include "Utilities/TextUtilities.h"
#include "Data/ArenaAllocator.h"

#include <imgui.h>
#include <imgui_impl_dx12.h>

#include "d3d12.h"
#include "d3dx12/d3dx12_pipeline_state_stream.h"
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <Windows.h>
#include <stdlib.h>

#define SR_MAX_CBV_SRV_UAV_DESCRIPTORS 65536
#define SR_MAX_SAMPLER_DESCRIPTORS     2048
#define SR_MAX_RTV_DESCRIPTORS         512
#define SR_MAX_DSV_DESCRIPTORS         64

extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = 618; }
extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\"; }

internal SRGFXDeviceVTable SRGFXDevice_DX12_VTable = {
	.destroy_device              = SRGFXDX12_DestroyDevice,
	.get_frame_index             = SRGFXDX12_GetFrameIndex,
	.create_swapchain            = SRGFXDX12_CreateSwapchain,
	.create_pipeline             = SRGFXDX12_CreatePipeline,
	.create_buffer               = SRGFXDX12_CreateBuffer,
	.create_texture              = SRGFXDX12_CreateTexture,
	.create_sampler              = SRGFXDX12_CreateSampler,
	.destroy_swapchain           = SRGFXDX12_DestroySwapchain,
	.destroy_pipeline            = SRGFXDX12_DestroyPipeline,
	.destroy_resource            = SRGFXDX12_DestroyResource,
	.bind_pipeline               = SRGFXDX12_BindPipeline,
	.bind_viewport               = SRGFXDX12_BindViewport,
	.bind_vertex_buffer          = SRGFXDX12_BindVertexBuffer,
	.bind_index_buffer           = SRGFXDX12_BindIndexBuffer,
	.bind_root_constant_buffer   = SRGFXDX12_BindRootConstantBuffer,
	.push_constants              = SRGFXDX12_PushConstants,
	.barrier                     = SRGFXDX12_Barrier,
	.begin_frame                 = SRGFXDX12_BeginFrame,
	.begin_command_list          = SRGFXDX12_BeginCommandList,
	.begin_render_pass_swapchain = SRGFXDX12_BeginRenderPassSwapchain,
	.begin_render_pass           = SRGFXDX12_BeginRenderPass,
	.end_render_pass_swapchain   = SRGFXDX12_EndRenderPassSwapchain,
	.end_render_pass             = SRGFXDX12_EndRenderPass,
	.submit_command_lists        = SRGFXDX12_SubmitCommandLists,
	.draw                        = SRGFXDX12_Draw,
	.draw_indexed                = SRGFXDX12_DrawIndexed,
	.draw_instanced              = SRGFXDX12_DrawInstanced,
	.dispatch_mesh               = SRGFXDX12_DispatchMesh,
	.get_descriptor_index_srv    = SRGFXDX12_GetDescriptorIndexSRV,
	.get_shader_compile_target   = SRGFXDX12_GetShaderCompileTarget,
	.wait_for_gpu                = SRGFXDX12_WaitForGPU,
	.flush_initial_uploads       = SRGFXDX12_FlushInitialUploads,
	.setup_imgui_init_info       = SRGFXDX12_SetupImGuiInitInfo
};

struct SRGFXDeviceDX12 {
	#ifdef _DEBUG
		ID3D12Debug* debug_interface;
		ID3D12DebugDevice* debug_device;
		IDXGIInfoQueue* dxgi_debug_info_queue;
	#endif
	IDXGIFactory2* dxgi_factory;
	IDXGIAdapter1* dxgi_adapter;
	ID3D12Device10* device;
	ID3D12CommandAllocator* upload_cmd_allocator;
	ID3D12GraphicsCommandList7* upload_cmd_list;
	ID3D12Fence* frame_fences[SRQueue_COUNT];
	ID3D12CommandAllocator* cmd_allocators[SRQueue_COUNT][SR_GFX_FRAMES_IN_FLIGHT];
	ID3D12CommandQueue* cmd_queues[SRQueue_COUNT];
	D3D12MA::Allocator* d3d12ma_allocator;

	SRCmdList_DX12 cmd_lists[SRQueue_COUNT][SR_GFX_FRAMES_IN_FLIGHT];
	SRDescriptorHeap_DX12* descriptor_heap_cbv_srv_uav;
	SRDescriptorHeap_DX12* descriptor_heap_sampler;
	SRDescriptorHeap_DX12* descriptor_heap_rtv;
	SRDescriptorHeap_DX12* descriptor_heap_dsv;
	SRDeviceCapabilities_DX12 device_capabilities;
	bool is_tearing_supported;
	bool is_upload_cmd_list_recording;

	SRArena* arena_general;
	SRArena* arena_upload;
	SRWindow* window;
	u64 frame_done_values[SRQueue_COUNT][SR_GFX_FRAMES_IN_FLIGHT];
	u32 frame_index;
	u32 image_index;
	u64 frame_counter;
};

internal void SRGFXDeviceDX12_CreateDebugInterface(SRGFXDeviceDX12* dev) {
#ifdef _DEBUG
	if (FAILED(D3D12GetDebugInterface(IID_PPV_ARGS(&dev->debug_interface)))) {
		SRLOG_WARN_CAT(SRLOG_CAT_DX12, "Failed to create base ID3D12Debug interface. Debug information will be limited");
		return;
	}

	dev->debug_interface->EnableDebugLayer();

	ID3D12Debug1* debug_interface_1 = nullptr;
	if (FAILED(dev->debug_interface->QueryInterface(IID_PPV_ARGS(&debug_interface_1)))) {
		SRLOG_WARN_CAT(SRLOG_CAT_DX12, "Failed to create ID3D12Debug1 interface. GBV/SCQV information will not be available");
		return;
	}

	debug_interface_1->SetEnableGPUBasedValidation(TRUE);
	debug_interface_1->SetEnableSynchronizedCommandQueueValidation(TRUE);
	debug_interface_1->Release();
#else
	return;
#endif
}

internal void SRGFXDeviceDX12_CreateDXGIDebugInterface(SRGFXDeviceDX12* dev) {
#ifdef _DEBUG
	if (FAILED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dev->dxgi_debug_info_queue)))) {
		SRLOG_WARN_CAT(SRLOG_CAT_DX12, "Failed to create DXGI debug interface. DXGI-related information will not be available");
		return;
	}

	HR(dev->dxgi_debug_info_queue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, TRUE));
	HR(dev->dxgi_debug_info_queue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, TRUE));
#else
	return;
#endif
}

internal void SRGFXDeviceDX12_CreateDXGIFactory(SRGFXDeviceDX12* dev) {
	UINT dxgi_factory_flags = 0;
	#ifdef _DEBUG
		dxgi_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
	#endif

	HR(CreateDXGIFactory2(dxgi_factory_flags, IID_PPV_ARGS(&dev->dxgi_factory)));

	IDXGIFactory5* dxgi_factory_5 = nullptr;
	HRESULT hr = dev->dxgi_factory->QueryInterface(IID_PPV_ARGS(&dxgi_factory_5));
	if (FAILED(hr)) { return; }

	BOOL allow_tearing = FALSE;
	hr = dxgi_factory_5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allow_tearing, sizeof(BOOL));
	dev->is_tearing_supported = SUCCEEDED(hr) && allow_tearing == TRUE;

	dxgi_factory_5->Release();
}

internal void SRGFXDeviceDX12_CreateDevice(SRGFXDeviceDX12* dev) {
	u32 picked_device_idx = ~0U;

	// NOTE: We prefer IDXGIFactory6 since it allows us to enumerate adapters
	// based on GPU preference. If it's not available, we pick a device with
	// EnumAdapters1 instead.
	IDXGIFactory6* dxgi_factory_6 = nullptr;
	bool is_dxgi_factory_6_avail = SUCCEEDED(dev->dxgi_factory->QueryInterface(IID_PPV_ARGS(&dxgi_factory_6)));

	char device_name_str8_data[128 * sizeof(WCHAR)];
	Str8 device_name_str8 = { (u8*)device_name_str8_data, 128 * sizeof(WCHAR) };

	for (UINT i = 0;; ++i) {
		IDXGIAdapter1* adapter = nullptr;
		HRESULT hr;

		if (is_dxgi_factory_6_avail) {
			hr = dxgi_factory_6->EnumAdapterByGpuPreference(
				i,
				DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
				IID_PPV_ARGS(&adapter)
			);
		}
		else {
			hr = dev->dxgi_factory->EnumAdapters1(i, &adapter);
		}

		if (FAILED(hr)) {
			break;
		}

		DXGI_ADAPTER_DESC1 adapterDesc;
		adapter->GetDesc1(&adapterDesc);
		Str16 device_name_str16 = { (u16*)adapterDesc.Description, 128 * sizeof(WCHAR) };
		Str16_ToStr8(&device_name_str16, &device_name_str8);

		if (adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
			SRLOG_DEBUG_CAT(SRLOG_CAT_DX12, "[GPU%u] %s REJECTED. Software/WARP adapter", i, device_name_str8.data);
			adapter->Release();
			continue;
		}

		ID3D12Device* device = nullptr;
		hr = D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device));
		if (FAILED(hr)) {
			SRLOG_DEBUG_CAT(SRLOG_CAT_DX12, "[GPU%u] %s REJECTED. Device creation failed", i, device_name_str8.data);
			adapter->Release();
			continue;
		}

		auto capabilities = SRDX12Helpers::query_device_capabilities(device);

		std::vector<std::string> missing;
		const auto REQUIRE = [&](bool condition, const char* str) {
			if (!condition) { missing.emplace_back(str); }
		};

		REQUIRE(capabilities.featureLevel >= D3D_FEATURE_LEVEL_12_0, "Feature Level >= 12.0");
		REQUIRE(capabilities.bindingTier >= D3D12_RESOURCE_BINDING_TIER_2, "Resource Binding Tier >= 2");
		REQUIRE(capabilities.rayTracingTier >= D3D12_RAYTRACING_TIER_1_1, "Ray Tracing Tier >= 1.1");
		REQUIRE(capabilities.meshShaderTier >= D3D12_MESH_SHADER_TIER_1, "Mesh Shader Tier >= 1");
		REQUIRE(capabilities.shaderModel >= D3D_SHADER_MODEL_6_5, "Shader Model >= 6.5");
		REQUIRE(capabilities.enhancedBarriersSupported, "Enhanced Barriers");

		if (!missing.empty()) {
			SRLOG_WARN_CAT(SRLOG_CAT_DX12, "[GPU%u] %s REJECTED. Missing %zu requirement(s):", i, device_name_str8.data, missing.size());

			for (const auto& str : missing) {
				SRLOG_WARN_CAT(SRLOG_CAT_DX12, "\t%s", str.c_str());
			}
			device->Release();
			adapter->Release();

			continue;
		}

		HR(device->QueryInterface(IID_PPV_ARGS(&dev->device)));
		dev->dxgi_adapter = adapter;

		#ifdef _DEBUG
			if (FAILED(dev->device->QueryInterface(IID_PPV_ARGS(&dev->debug_device)))) {
				SRLOG_DEBUG_CAT(SRLOG_CAT_DX12, "ID3D12DebugDevice not available. D3D12 object reporting disabled");
			}
		#endif

		dev->device_capabilities = capabilities;
		picked_device_idx = i;
		device->Release();

		break;
	}

	if (is_dxgi_factory_6_avail) {
		dxgi_factory_6->Release();
	}

	if (picked_device_idx == ~0) {
		SRLOG_CRITICAL_CAT(SRLOG_CAT_DX12, "No suitable GPU found");
		throw std::runtime_error("DX12 ERROR: No suitable GPU found");
	}

	SRLOG_INFO_CAT(SRLOG_CAT_DX12, "Picked [GPU%u] %s", picked_device_idx, device_name_str8.data);
}

internal void SRGFXDeviceDX12_CreateMemoryAllocator(SRGFXDeviceDX12* dev) {
	D3D12MA::ALLOCATOR_DESC allocatorDesc = {
		.Flags = D3D12MA_RECOMMENDED_ALLOCATOR_FLAGS,
		.pDevice = dev->device,
		.pAdapter = dev->dxgi_adapter
	};

	HR(D3D12MA::CreateAllocator(&allocatorDesc, &dev->d3d12ma_allocator));
}

internal void SRGFXDeviceDX12_CreateCommandAllocators(SRGFXDeviceDX12* dev) {
	for (u32 f = 0; f < SR_GFX_FRAMES_IN_FLIGHT; ++f) {
		// Universal command allocators (direct)
		HR(dev->device->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(&dev->cmd_allocators[SRQueue_Universal][f])
		));

		// TODO: Create command allocators for other queue types too. We will need
		// it eventually.

		// Compute (TODO)

		// Copy command allocators (TODO)
	}

	// TEMPORARY
	HR(dev->device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_COPY,
		IID_PPV_ARGS(&dev->upload_cmd_allocator)
	));
	HR(dev->device->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_COPY,
		dev->upload_cmd_allocator,
		nullptr,
		IID_PPV_ARGS(&dev->upload_cmd_list)
	));
	dev->upload_cmd_list->Close();
}

internal void SRGFXDeviceDX12_CreateCommandQueues(SRGFXDeviceDX12* dev) {
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc = {
		.Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
		.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
		.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
		.NodeMask = 0
	};

	HR(dev->device->CreateCommandQueue(
		&commandQueueDesc,
		IID_PPV_ARGS(&dev->cmd_queues[SRQueue_Universal])
	));

	commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;

	HR(dev->device->CreateCommandQueue(
		&commandQueueDesc,
		IID_PPV_ARGS(&dev->cmd_queues[SRQueue_Copy])
	));

	// TODO: Create command queues for other queue types too. We will need it
	// eventually.
}

internal void SRGFXDeviceDX12_CreateSyncObjects(SRGFXDeviceDX12* dev) {
	for (u32 q = 0; q < SRQueue_COUNT; ++q) {
		HR(dev->device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&dev->frame_fences[q])));
	}
}

internal void SRGFXDeviceDX12_CreateDescriptorHeaps(SRGFXDeviceDX12* dev) {
	dev->descriptor_heap_cbv_srv_uav = SRDescriptorHeap_DX12_Create(dev->arena_general, dev->device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, SR_MAX_CBV_SRV_UAV_DESCRIPTORS);
	dev->descriptor_heap_sampler     = SRDescriptorHeap_DX12_Create(dev->arena_general, dev->device, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, SR_MAX_SAMPLER_DESCRIPTORS);
	dev->descriptor_heap_rtv         = SRDescriptorHeap_DX12_Create(dev->arena_general, dev->device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, SR_MAX_RTV_DESCRIPTORS);
	dev->descriptor_heap_dsv         = SRDescriptorHeap_DX12_Create(dev->arena_general, dev->device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, SR_MAX_DSV_DESCRIPTORS);
}

// ------------------------------ Public API ------------------------------
void SRGFXDX12_CreateDevice(SRWindow* window, SRGFXDevice* device) {
	SRGFXDeviceDX12* dev_dx12 = (SRGFXDeviceDX12*)malloc(sizeof(SRGFXDeviceDX12));
	assert(dev_dx12 != nullptr);
	ZeroMemory(dev_dx12, sizeof(*dev_dx12));

	device->internalState = dev_dx12;
	device->vtbl = &SRGFXDevice_DX12_VTable;

	dev_dx12->window = window;
	dev_dx12->arena_general = SRArena_Create(Gigabytes(1));
	dev_dx12->arena_upload = SRArena_Create(Gigabytes(1));

	SRGFXDeviceDX12_CreateDebugInterface(dev_dx12);
	SRGFXDeviceDX12_CreateDXGIDebugInterface(dev_dx12);
	SRGFXDeviceDX12_CreateDXGIFactory(dev_dx12);
	SRGFXDeviceDX12_CreateDevice(dev_dx12);
	SRGFXDeviceDX12_CreateMemoryAllocator(dev_dx12);
	SRGFXDeviceDX12_CreateCommandAllocators(dev_dx12);
	SRGFXDeviceDX12_CreateCommandQueues(dev_dx12);
	SRGFXDeviceDX12_CreateSyncObjects(dev_dx12);
	SRGFXDeviceDX12_CreateDescriptorHeaps(dev_dx12);
}

void SRGFXDX12_DestroyDevice(SRGFXDevice* device) {
	SRGFXDeviceDX12* dev = (SRGFXDeviceDX12*)device->internalState;
	SRDescriptorHeap_DX12_Destroy(dev->descriptor_heap_cbv_srv_uav);
	SRDescriptorHeap_DX12_Destroy(dev->descriptor_heap_sampler);
	SRDescriptorHeap_DX12_Destroy(dev->descriptor_heap_rtv);
	SRDescriptorHeap_DX12_Destroy(dev->descriptor_heap_dsv);
	SRArena_Destroy(dev->arena_general);
	SRArena_Destroy(dev->arena_upload);

	for (u32 q = 0; q < SRQueue_COUNT; ++q) {
		dev->frame_fences[q]->Release();
	}

	dev->cmd_queues[SRQueue_Universal]->Release();
	dev->cmd_queues[SRQueue_Copy]->Release();

	for (u32 f = 0; f < SR_GFX_FRAMES_IN_FLIGHT; ++f) {
		dev->cmd_allocators[SRQueue_Universal][f]->Release();
	}

	for (u32 q = 0; q < SRQueue_COUNT; ++q) {
		for (u32 f = 0; f < SR_GFX_FRAMES_IN_FLIGHT; ++f) {
			SRCmdList_DX12* cmd_list = &dev->cmd_lists[q][f];

			if (cmd_list->graphicsCmdList) {
				cmd_list->graphicsCmdList->Release();
			}
		}
	}

	dev->dxgi_factory->Release();
	dev->upload_cmd_list->Release();
	dev->upload_cmd_allocator->Release();
	dev->d3d12ma_allocator->Release();
	dev->dxgi_adapter->Release();
	dev->device->Release();

	#ifdef _DEBUG
		dev->debug_device->Release();
		dev->dxgi_debug_info_queue->Release();
		dev->debug_interface->Release();
	#endif

	free(dev);
	device->internalState = nullptr;
}

u32 SRGFXDX12_GetFrameIndex(SRGFXDevice* device) {
	auto* dev = (SRGFXDeviceDX12*)device->internalState;
	return dev->frame_index;
}

void SRGFXDX12_CreateSwapchain(SRGFXDevice* device, const SRSwapchainInfo* info, SRSwapchain* swapchain) {
	assert(info->numBuffers <= SR_MAX_SWAPCHAIN_IMAGES);
	auto* dev = (SRGFXDeviceDX12*)device->internalState;

	// Swapchain recreation
	if (swapchain->internalState != nullptr) {
		auto* internal_swapchain = to_dx12_internal(swapchain);
		SRGFX_WaitForGPU(device);

		for (u64 i = 0; i < internal_swapchain->imageCount; ++i) {
			internal_swapchain->images[i]->Release();
		}

		HR(internal_swapchain->swapchain->ResizeBuffers(
			info->numBuffers,
			info->width,
			info->height,
			to_dx12_format(info->format),
			dev->is_tearing_supported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0
		));
		internal_swapchain->imageCount = info->numBuffers;

		D3D12_RENDER_TARGET_VIEW_DESC rtv_desc = {
			.Format = to_dx12_format(info->format),
			.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D
		};

		// TODO: Right now we assume that we don't resize the swapchain in terms of backbuffers
		// And this will break in the case that we would go from double-buffer to triple-buffer
		// for example.
		for (UINT i = 0; i < info->numBuffers; ++i) {
			HR(internal_swapchain->swapchain->GetBuffer(i, IID_PPV_ARGS(&internal_swapchain->images[i])));

			dev->device->CreateRenderTargetView(
				internal_swapchain->images[i],
				&rtv_desc,
				SRDescriptorHeap_DX12_GetCPUHandle(dev->descriptor_heap_rtv, internal_swapchain->rtvDescriptors[i])
			);
		}

		return;
	}

	auto* internalSwapchain = SRArena_PushStructZero(dev->arena_general, SRSwapchain_DX12);
	swapchain->info = *info;
	swapchain->internalState = internalSwapchain;

	// TODO: Allow for swapchain recreation
	DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {
		.Width = info->width,
		.Height = info->height,
		.Format = to_dx12_format(info->format),
		.Stereo = FALSE,
		.SampleDesc = { .Count = 1U, .Quality = 0U },
		.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
		.BufferCount = info->numBuffers,
		.Scaling = DXGI_SCALING_NONE,
		.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
		.AlphaMode = DXGI_ALPHA_MODE_IGNORE,
		.Flags = dev->is_tearing_supported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0U
	};

	HWND windowHandle = (HWND)SRWindow_GetInternalHandle(dev->window);
	IDXGISwapChain1* dxgiSwapchain1;
	HR(dev->dxgi_factory->CreateSwapChainForHwnd(
		dev->cmd_queues[SRQueue_Universal],
		windowHandle,
		&swapchainDesc,
		nullptr,
		nullptr,
		&dxgiSwapchain1
	));
	HR(dxgiSwapchain1->QueryInterface(IID_PPV_ARGS(&internalSwapchain->swapchain)));
	HR(dev->dxgi_factory->MakeWindowAssociation(windowHandle, DXGI_MWA_NO_ALT_ENTER));
	dxgiSwapchain1->Release();

	internalSwapchain->imageCount = info->numBuffers;

	for (u32 i = 0; i < internalSwapchain->imageCount; ++i) {
		HR(internalSwapchain->swapchain->GetBuffer(i, IID_PPV_ARGS(&internalSwapchain->images[i])));

		SRDescriptorIndex rtvIndex = SRDescriptorHeap_DX12_GetNextIndex(dev->descriptor_heap_rtv);
		dev->device->CreateRenderTargetView(
			internalSwapchain->images[i],
			nullptr,
			SRDescriptorHeap_DX12_GetCPUHandle(dev->descriptor_heap_rtv, rtvIndex)
		);
		internalSwapchain->rtvDescriptors[i] = rtvIndex;
	}
}

void SRGFXDX12_CreatePipeline(SRGFXDevice* device, const SRPipelineInfo* info, SRPipeline* pipeline) {
	auto* dev = (SRGFXDeviceDX12*)device->internalState;
	auto* internalPipeline = SRArena_PushStructZero(dev->arena_general, SRPipeline_DX12);
	pipeline->info = *info;
	pipeline->internalState = internalPipeline;

	struct PSOStream {
		CD3DX12_PIPELINE_STATE_STREAM_VS                    vertexShader;
		CD3DX12_PIPELINE_STATE_STREAM_PS                    pixelShader;
		CD3DX12_PIPELINE_STATE_STREAM_MS                    meshShader;
		CD3DX12_PIPELINE_STATE_STREAM_AS                    taskShader;
		CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER            rasterizerState;
		CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL1        depthStencilState;
		CD3DX12_PIPELINE_STATE_STREAM_BLEND_DESC            blendDesc;
		CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY    primitiveTopology;
		CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT          inputLayout;
		CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT  depthStencilFormat;
		CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS formats;
		CD3DX12_PIPELINE_STATE_STREAM_SAMPLE_DESC           sampleDesc;
		CD3DX12_PIPELINE_STATE_STREAM_SAMPLE_MASK           sampleMask;
		CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE        rootSignature;
	} psoStream = {};

	D3D12_ROOT_PARAMETER1 rootConstant = {
		.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
		.Constants = { .ShaderRegister = 0, .RegisterSpace = 0, .Num32BitValues = 32 /* 128 bytes */ },
		.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
	};
	D3D12_ROOT_PARAMETER1 perFrameCBV = {
		.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV,
		.Descriptor = {
			.ShaderRegister = 1,
			.RegisterSpace = 0,
			.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE
		},
		.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
	};
	D3D12_ROOT_PARAMETER1 rootParameters[] = {
		rootConstant, // Root Parameter 0
		perFrameCBV   // Root Parameter 1
	};

	D3D12_VERSIONED_ROOT_SIGNATURE_DESC root_sig_desc {
		.Version = D3D_ROOT_SIGNATURE_VERSION_1_1,
		.Desc_1_1 = {
			.NumParameters = static_cast<UINT>(std::size(rootParameters)),
			.pParameters = rootParameters,
			.NumStaticSamplers = 0,
			.pStaticSamplers = nullptr,
			.Flags = (
				D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED |
				D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED
			)
		}
	};

	// TODO: Improve this logic
	if (!info->meshShader) {
		root_sig_desc.Desc_1_1.Flags |= D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	}

	ID3DBlob* root_sig_blob = nullptr;
	ID3DBlob* root_sig_blob_err = nullptr;
	HR(D3D12SerializeVersionedRootSignature(&root_sig_desc, &root_sig_blob, &root_sig_blob_err));
	HR(dev->device->CreateRootSignature(0,
		root_sig_blob->GetBufferPointer(),
		root_sig_blob->GetBufferSize(),
		IID_PPV_ARGS(&internalPipeline->rootSignature)
	));
	psoStream.rootSignature = internalPipeline->rootSignature;

	if (root_sig_blob) {
		root_sig_blob->Release();
	}
	if (root_sig_blob_err) {
		root_sig_blob_err->Release();
	}

	if (info->vertexShader != nullptr) {
		psoStream.vertexShader = { info->vertexShader->data, info->vertexShader->size, };
	}
	if (info->pixelShader != nullptr) {
		psoStream.pixelShader = { info->pixelShader->data, info->pixelShader->size };
	}
	if (info->meshShader != nullptr) {
		psoStream.meshShader = { info->meshShader->data, info->meshShader->size };
	}
	if (info->taskShader != nullptr) {
		psoStream.taskShader = { info->taskShader->data, info->taskShader->size };
	}

	// Rasterizer state
	CD3DX12_RASTERIZER_DESC rasterizerDesc = {};
	rasterizerDesc.FillMode = to_dx12_fill_mode(info->rasterizerState.fillMode);
	rasterizerDesc.CullMode = to_dx12_cull_mode(info->rasterizerState.cullMode);
	rasterizerDesc.FrontCounterClockwise = info->rasterizerState.frontCW ? TRUE : FALSE;
	rasterizerDesc.DepthBias = info->rasterizerState.depthBias;
	rasterizerDesc.DepthBiasClamp = info->rasterizerState.depthBiasClamp;
	rasterizerDesc.SlopeScaledDepthBias = info->rasterizerState.slopeScaledDepthBias;
	rasterizerDesc.DepthClipEnable = info->rasterizerState.depthClipEnable ? TRUE : FALSE;
	rasterizerDesc.MultisampleEnable = info->rasterizerState.multisampleEnable ? TRUE : FALSE;
	rasterizerDesc.AntialiasedLineEnable = info->rasterizerState.antialisedLineEnable ? TRUE : FALSE;
	rasterizerDesc.ForcedSampleCount = 0U;
	rasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
	psoStream.rasterizerState = rasterizerDesc;

	// Depth stencil state
	CD3DX12_DEPTH_STENCIL_DESC1 depthStencilDesc = {};
	depthStencilDesc.DepthEnable = info->depthStencilState.depthEnable ? TRUE : FALSE;
	depthStencilDesc.DepthWriteMask = to_dx12_depth_write_mask(info->depthStencilState.depthWriteMask);
	depthStencilDesc.DepthFunc = to_dx12_comparison_func(info->depthStencilState.depthFunction);
	depthStencilDesc.StencilEnable = info->depthStencilState.stencilEnable ? TRUE : FALSE;
	depthStencilDesc.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	depthStencilDesc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
	depthStencilDesc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	depthStencilDesc.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	depthStencilDesc.DepthBoundsTestEnable = FALSE;
	psoStream.depthStencilState = depthStencilDesc;

	// Blend state
	CD3DX12_BLEND_DESC blendDesc = {};
	blendDesc.AlphaToCoverageEnable = info->blendState.alphaToCoverage ? TRUE : FALSE;
	blendDesc.IndependentBlendEnable = info->blendState.independentBlend ? TRUE : FALSE;
	for (size_t i = 0; i < 8; ++i) {
		const auto& blendState = info->blendState.renderTargetBlendStates[i];
		auto& dx12BlendState = blendDesc.RenderTarget[i];

		dx12BlendState.BlendEnable = blendState.blendEnable ? TRUE : FALSE;
		dx12BlendState.SrcBlend = to_dx12_blend(blendState.srcBlend);
		dx12BlendState.DestBlend = to_dx12_blend(blendState.dstBlend);
		dx12BlendState.BlendOp = to_dx12_blend_op(blendState.blendOp);
		dx12BlendState.SrcBlendAlpha = to_dx12_alpha_blend(blendState.srcBlendAlpha);
		dx12BlendState.DestBlendAlpha = to_dx12_alpha_blend(blendState.dstBlendAlpha);
		dx12BlendState.BlendOpAlpha = to_dx12_blend_op(blendState.blendOpAlpha);
		dx12BlendState.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	}
	psoStream.blendDesc = blendDesc;

	// Primitive topology
	psoStream.primitiveTopology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	// Input layout
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc = {};
	inputLayoutDesc.NumElements = static_cast<UINT>(info->inputLayout.num_elements);
	std::vector<D3D12_INPUT_ELEMENT_DESC> inputElements;
	inputElements.reserve(info->inputLayout.num_elements);

	for (size_t i = 0; i < info->inputLayout.num_elements; ++i) {
		const auto& element = info->inputLayout.elements[i];
		D3D12_INPUT_ELEMENT_DESC dx12Element = {
			.SemanticName = element.name,
			.SemanticIndex = 0U, // TODO: Pretty certain this doesn't matter
			.Format = to_dx12_format(element.format),
			.InputSlot = 0U, // NOTE: No more than 1 vertex buffer will be bound at a time, so this will always be 0 in our case
			.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT,
			.InputSlotClass = to_dx12_input_class(element.inputClass),
			.InstanceDataStepRate = 0U, // TODO: Fix if per-instance data is used, right now it will not work with 0
		};
		inputElements.push_back(dx12Element);
	}

	inputLayoutDesc.pInputElementDescs = inputElements.data();
	psoStream.inputLayout = inputLayoutDesc;

	// Depth stencil format
	psoStream.depthStencilFormat = to_dx12_format(info->depthStencilFormat);

	// RTV formats
	D3D12_RT_FORMAT_ARRAY rtvFormats = {};
	rtvFormats.NumRenderTargets = info->numRenderTargets;
	for (size_t i = 0; i < info->numRenderTargets; ++i) {
		rtvFormats.RTFormats[i] = to_dx12_format(info->renderTargetFormats[i]);
	}
	psoStream.formats = rtvFormats;

	// Sample desc
	DXGI_SAMPLE_DESC sampleDesc = {};
	sampleDesc.Count = 1U;
	sampleDesc.Quality = 0U;
	psoStream.sampleDesc = sampleDesc;

	// Sample mask
	psoStream.sampleMask = UINT_MAX;

	D3D12_PIPELINE_STATE_STREAM_DESC psoStreamDesc = {
		.SizeInBytes = sizeof(PSOStream),
		.pPipelineStateSubobjectStream = &psoStream
	};

	HR(dev->device->CreatePipelineState(&psoStreamDesc, IID_PPV_ARGS(&internalPipeline->pipeline)));
}

void SRGFXDX12_CreateBuffer(SRGFXDevice* device, const SRBufferInfo* info, SRBuffer* buffer, const void* data) {
	auto* dev = (SRGFXDeviceDX12*)device->internalState;
	auto* internal_buffer = SRArena_PushStructZero(dev->arena_general, SRBuffer_DX12);

	buffer->type = SRResourceType::Buffer;
	buffer->info = *info;
	buffer->internalState = internal_buffer;
	buffer->mappedData = nullptr;
	buffer->mappedSize = 0;

	D3D12_RESOURCE_DESC1 resource_desc = {
		.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
		.Alignment = 0,
		.Width = info->size,
		.Height = 1,
		.DepthOrArraySize = 1,
		.MipLevels = 1,
		.Format = DXGI_FORMAT_UNKNOWN,
		.SampleDesc = { .Count = 1, .Quality = 0 },
		.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
		.Flags = D3D12_RESOURCE_FLAG_NONE
	};

	if (has_flag(info->bindFlags, SRBindFlag::UnorderedAccess)) {
		resource_desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	}
	// TODO: Look into whether or not this is always correct
	if (!has_flag(info->bindFlags, SRBindFlag::ShaderResource)) {
		resource_desc.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
	}

	D3D12MA::ALLOCATION_DESC alloc_desc = {
		.HeapType = D3D12_HEAP_TYPE_DEFAULT,
	};

	switch (info->usage) {
	case SRUsage::Default:
		alloc_desc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
		break;
	case SRUsage::Upload:
		alloc_desc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
		break;
	}
	HR(dev->d3d12ma_allocator->CreateResource3(
		&alloc_desc,
		&resource_desc,
		D3D12_BARRIER_LAYOUT_UNDEFINED,
		nullptr,
		0,
		nullptr,
		&internal_buffer->allocation,
		IID_NULL, nullptr
	));

	if (info->usage == SRUsage::Default && data != nullptr) {
		// Staging buffer
		SRBufferInfo stagingBufferInfo = *info;
		stagingBufferInfo.usage = SRUsage::Upload;
		stagingBufferInfo.bindFlags = SRBindFlag::None;
		stagingBufferInfo.miscFlags = SRMiscFlag::None;

		SRBuffer stagingBuffer;
		SRGFXDX12_CreateBuffer(device, &stagingBufferInfo, &stagingBuffer, data);
		auto* internalStagingBuffer = to_dx12_internal(&stagingBuffer);

		//dev->m_PendingUploadResources.push_back(internalStagingBuffer->allocation);
		D3D12MA::Allocation** upload = SRArena_PushStructZero(dev->arena_upload, D3D12MA::Allocation*);
		*upload = internalStagingBuffer->allocation;

		// Copy staging buffer into target buffer
		if (!dev->is_upload_cmd_list_recording) {
			HR(dev->upload_cmd_allocator->Reset());
			HR(dev->upload_cmd_list->Reset(dev->upload_cmd_allocator, nullptr));

			dev->is_upload_cmd_list_recording = true;
		}

		ID3D12Resource* srcResource = internalStagingBuffer->allocation->GetResource();
		ID3D12Resource* dstResource = internal_buffer->allocation->GetResource();
		dev->upload_cmd_list->CopyResource(dstResource, srcResource);
	}
	else if (info->usage == SRUsage::Upload) {
		internal_buffer->allocation->GetResource()->Map(0, nullptr, &buffer->mappedData);
		buffer->mappedSize = info->size;

		if (data != nullptr) {
			memcpy(buffer->mappedData, data, info->size);
		}

		// NOTE: We always perform persistent mappings, so no unmap is necessary
	}

	// Descriptors
	// TODO: UAV
	// SRV
	if (has_flag(info->bindFlags, SRBindFlag::ShaderResource)) {
		if (has_flag(info->miscFlags, SRMiscFlag::StructuredBuffer)) {
			D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {
				.Format = DXGI_FORMAT_UNKNOWN,
				.ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
				.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
				.Buffer = {
					.FirstElement = 0,
					.NumElements = static_cast<UINT>(info->size / info->stride),
					.StructureByteStride = info->stride,
					.Flags = D3D12_BUFFER_SRV_FLAG_NONE
				}
			};

			SRDescriptorIndex srv_index = SRDescriptorHeap_DX12_GetNextIndex(dev->descriptor_heap_cbv_srv_uav);
			internal_buffer->srvDescriptor = srv_index;

			dev->device->CreateShaderResourceView(
				internal_buffer->allocation->GetResource(),
				&srv_desc,
				SRDescriptorHeap_DX12_GetCPUHandle(dev->descriptor_heap_cbv_srv_uav, srv_index)
			);
		}
	}
	// TODO: CBV
}

void SRGFXDX12_CreateTexture(SRGFXDevice* device, const SRTextureInfo* info, SRTexture* texture, const SRSubresourceData* data) {
	assert(info->usage == SRUsage::Default);
	auto* dev = (SRGFXDeviceDX12*)device->internalState;

	SRTexture_DX12* internal_texture;

	if (texture->internalState) {
		internal_texture = to_dx12_internal(texture);
		assert(internal_texture->allocation == nullptr);
	}
	else {
		internal_texture = SRArena_PushStructZero(dev->arena_general, SRTexture_DX12);
	}

	texture->info = *info;
	texture->internalState = internal_texture;
	texture->type = SRResourceType::Texture;

	D3D12_RESOURCE_DESC1 resource_desc = {
		.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
		.Alignment = 0,
		.Width = static_cast<UINT64>(info->width),
		.Height = info->height,
		.DepthOrArraySize = static_cast<UINT16>(info->depth),
		.MipLevels = static_cast<UINT16>(info->mipLevels),
		.Format = to_dx12_format(info->format),
		.SampleDesc = { .Count = info->sampleCount, .Quality = 0 },
		.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
		.Flags = D3D12_RESOURCE_FLAG_NONE
	};
	D3D12_BARRIER_LAYOUT initial_layout = D3D12_BARRIER_LAYOUT_UNDEFINED;
	if (data && data->data) {
		initial_layout = D3D12_BARRIER_LAYOUT_COMMON;
	}

	// Bind flags
	if (has_flag(info->bindFlags, SRBindFlag::DepthStencil)) {
		resource_desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	}
	if (has_flag(info->bindFlags, SRBindFlag::UnorderedAccess)) {
		resource_desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	}
	if (has_flag(info->bindFlags, SRBindFlag::RenderTarget)) {
		resource_desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	}

	D3D12MA::ALLOCATION_DESC alloc_desc = {
		.HeapType = D3D12_HEAP_TYPE_DEFAULT,
	};
	HR(dev->d3d12ma_allocator->CreateResource3(
		&alloc_desc,
		&resource_desc,
		initial_layout,
		nullptr,
		0,
		nullptr,
		&internal_texture->allocation,
		IID_NULL, nullptr
	));
	// ^NOTE: D3D12MA is broken when it comes to tight alignment, since it occasionally generates internal error
	// for some resources. I.e. ignore all reported errors stemming from this function

	// TODO: Implement subresource data copying
	u32 subres_count = info->mipLevels;
	if (data && data->data && subres_count > 0) {
		SRArenaMarker m = SRArena_GetMarker(dev->arena_general);
		auto* subres_footprints = SRArena_PushArrayZero(dev->arena_general, D3D12_PLACED_SUBRESOURCE_FOOTPRINT, subres_count);
		u32* subres_num_rows = SRArena_PushArrayZero(dev->arena_general, u32, subres_count);
		u64* subres_row_sizes_in_bytes = SRArena_PushArrayZero(dev->arena_general, u64, subres_count);
		u64 subres_total_size = 0;

		dev->device->GetCopyableFootprints1(
			&resource_desc,
			0,
			subres_count,
			0,
			subres_footprints,
			subres_num_rows,
			subres_row_sizes_in_bytes,
			&subres_total_size
		);

		// Staging buffer
		SRBufferInfo staging_buffer_info = {
			.size = subres_total_size,
			.usage = SRUsage::Upload,
		};

		SRBuffer staging_buffer;
		SRGFXDX12_CreateBuffer(device, &staging_buffer_info, &staging_buffer, data->data);
		auto* internal_staging_buffer = to_dx12_internal(&staging_buffer);

		D3D12MA::Allocation** upload = SRArena_PushStructZero(dev->arena_upload, D3D12MA::Allocation*);
		*upload = internal_staging_buffer->allocation;

		// Copy staging buffer into target buffer
		if (!dev->is_upload_cmd_list_recording) {
			HR(dev->upload_cmd_allocator->Reset());
			HR(dev->upload_cmd_list->Reset(dev->upload_cmd_allocator, nullptr));

			dev->is_upload_cmd_list_recording = true;
		}

		// TODO: Get this working for multiple subresources, right now only single-mip works
		for (u32 i = 0; i < subres_count; ++i) {
			auto* footprint = &subres_footprints[i];
			u32 num_rows = subres_num_rows[i];
			u64 row_size_in_bytes = subres_row_sizes_in_bytes[i];

			D3D12_SUBRESOURCE_DATA src_data = {
				.pData = data->data,
				.RowPitch = (LONG_PTR)data->rowPitch,
				.SlicePitch = (LONG_PTR)data->slicePitch
			};
			D3D12_MEMCPY_DEST dst_data = {
				.pData = (void*)((u64)staging_buffer.mappedData + footprint->Offset),
				.RowPitch = (SIZE_T)footprint->Footprint.RowPitch,
				.SlicePitch = (SIZE_T)footprint->Footprint.RowPitch * (SIZE_T)num_rows
			};

			for (UINT z = 0; z < footprint->Footprint.Depth; ++z) {
				auto* dst_slice = (u8*)dst_data.pData + dst_data.SlicePitch * z;
				auto* src_slice = (u8*)src_data.pData + src_data.SlicePitch * (LONG_PTR)z;

				for (UINT y = 0; y < num_rows; ++y) {
					memcpy(dst_slice + dst_data.RowPitch * y, src_slice + src_data.RowPitch * (LONG_PTR)y, row_size_in_bytes);
				}
			}

			D3D12_TEXTURE_COPY_LOCATION src = {
				.pResource = internal_staging_buffer->allocation->GetResource(),
				.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
				.PlacedFootprint = *footprint, // TODO: If we ever want a ring buffer, we will have to also change the offset
			};
			D3D12_TEXTURE_COPY_LOCATION dst = {
				.pResource = internal_texture->allocation->GetResource(),
				.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
				.SubresourceIndex = static_cast<UINT>(i)
			};

			dev->upload_cmd_list->CopyTextureRegion(
				&dst,
				0U,
				0U,
				0U,
				&src,
				nullptr
			);
		}

		SRArena_PopToMarker(dev->arena_general, m);
	}

	// RTV Descriptor
	if (has_flag(info->bindFlags, SRBindFlag::RenderTarget)) {
		D3D12_RENDER_TARGET_VIEW_DESC rtv_desc = {
			.Format = resource_desc.Format,
			.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D
		};

		SRDescriptorIndex rtv_index = SRDescriptorHeap_DX12_GetNextIndex(dev->descriptor_heap_rtv);
		internal_texture->rtvDescriptor = rtv_index;

		dev->device->CreateRenderTargetView(
			internal_texture->allocation->GetResource(),
			&rtv_desc,
			SRDescriptorHeap_DX12_GetCPUHandle(dev->descriptor_heap_rtv, rtv_index)
		);
	}

	// DSV Descriptors
	if (has_flag(info->bindFlags, SRBindFlag::DepthStencil)) {
		D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc = {
			.Format = resource_desc.Format,
			.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
			.Flags = D3D12_DSV_FLAG_NONE
		};

		D3D12_DEPTH_STENCIL_VIEW_DESC dsv_read_only_desc = {
			.Format = resource_desc.Format,
			.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
			.Flags = D3D12_DSV_FLAG_READ_ONLY_DEPTH
		};

		SRDescriptorIndex dsv_index = SRDescriptorHeap_DX12_GetNextIndex(dev->descriptor_heap_dsv);
		SRDescriptorIndex dsv_read_only_index = SRDescriptorHeap_DX12_GetNextIndex(dev->descriptor_heap_dsv);
		internal_texture->dsvDescriptor = dsv_index;
		internal_texture->dsvReadOnlyDescriptor = dsv_read_only_index;

		dev->device->CreateDepthStencilView(
			internal_texture->allocation->GetResource(),
			&dsv_desc,
			SRDescriptorHeap_DX12_GetCPUHandle(dev->descriptor_heap_dsv, dsv_index)
		);
		dev->device->CreateDepthStencilView(
			internal_texture->allocation->GetResource(),
			&dsv_read_only_desc,
			SRDescriptorHeap_DX12_GetCPUHandle(dev->descriptor_heap_dsv, dsv_read_only_index)
		);
	}

	// SRV Descriptor
	if (has_flag(info->bindFlags, SRBindFlag::ShaderResource)) {
		DXGI_FORMAT srvFormat = resource_desc.Format;

		if (info->format == SRFormat::D32_FLOAT) {
			srvFormat = DXGI_FORMAT_R32_FLOAT;
		}
		else if (info->format == SRFormat::D16_UNORM) {
			srvFormat = DXGI_FORMAT_R16_UNORM;
		}

		D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {
			.Format = srvFormat,
			.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
			.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
			.Texture2D = {
				.MostDetailedMip = 0,
				.MipLevels = info->mipLevels,
			}
		};

		SRDescriptorIndex srv_index = SRDescriptorHeap_DX12_GetNextIndex(dev->descriptor_heap_cbv_srv_uav);
		internal_texture->srvDescriptor = srv_index;

		dev->device->CreateShaderResourceView(
			internal_texture->allocation->GetResource(),
			&srv_desc,
			SRDescriptorHeap_DX12_GetCPUHandle(dev->descriptor_heap_cbv_srv_uav, srv_index)
		);
	}
}

void SRGFXDX12_CreateSampler(SRGFXDevice* device, const SRSamplerInfo* info, SRSampler* sampler) {
	auto* dev = (SRGFXDeviceDX12*)device->internalState;
	auto* internal_sampler = SRArena_PushStructZero(dev->arena_general, SRSampler_DX12);

	sampler->info = *info;
	sampler->type = SRResourceType::Sampler;
	sampler->internalState = internal_sampler;

	D3D12_SAMPLER_DESC sampler_desc = {
		.Filter = to_dx12_filter(info->filter),
		.AddressU = to_dx12_texture_address_mode(info->addressU),
		.AddressV = to_dx12_texture_address_mode(info->addressV),
		.AddressW = to_dx12_texture_address_mode(info->addressW),
		.MipLODBias = info->mipLODBias,
		.MaxAnisotropy = info->maxAnisotropy,
		.ComparisonFunc = to_dx12_comparison_func(info->comparisonFunc),
		.MinLOD = info->minLOD,
		.MaxLOD = std::numeric_limits<float>::max()
	};

	switch (info->borderColor) {
	case SRBorderColor::OpaqueBlack:
	{
		sampler_desc.BorderColor[0] = 0.0F;
		sampler_desc.BorderColor[1] = 0.0F;
		sampler_desc.BorderColor[2] = 0.0F;
		sampler_desc.BorderColor[3] = 1.0F;
	}
	break;
	case SRBorderColor::OpaqueWhite:
	{
		sampler_desc.BorderColor[0] = 1.0F;
		sampler_desc.BorderColor[1] = 1.0F;
		sampler_desc.BorderColor[2] = 1.0F;
		sampler_desc.BorderColor[3] = 1.0F;
	}
	break;
	default:
	{
		sampler_desc.BorderColor[0] = 0.0F;
		sampler_desc.BorderColor[1] = 0.0F;
		sampler_desc.BorderColor[2] = 0.0F;
		sampler_desc.BorderColor[3] = 0.0F;
	}
	break;
	}

	SRDescriptorIndex index = SRDescriptorHeap_DX12_GetNextIndex(dev->descriptor_heap_sampler);
	internal_sampler->samplerDescriptor = index;
	dev->device->CreateSampler(
		&sampler_desc,
		SRDescriptorHeap_DX12_GetCPUHandle(dev->descriptor_heap_sampler, index)
	);
}

void SRGFXDX12_DestroySwapchain(SRGFXDevice* device, SRSwapchain* swapchain) {
	auto* internalSwapchain = to_dx12_internal(swapchain);

	internalSwapchain->swapchain->Release();
	for (u64 i = 0; i < internalSwapchain->imageCount; ++i) {
		internalSwapchain->images[i]->Release();
	}

	swapchain->internalState = nullptr;
}

void SRGFXDX12_DestroyPipeline(SRGFXDevice* device, SRPipeline* pipeline) {
	auto* internalPipeline = to_dx12_internal(pipeline);

	internalPipeline->pipeline->Release();
	internalPipeline->rootSignature->Release();
	pipeline->internalState = nullptr;
}

void SRGFXDX12_DestroyResource(SRGFXDevice* device, SRResource* resource) {
	auto* dev = (SRGFXDeviceDX12*)device->internalState;

	// TODO: Remove sampler as resource type
	if (resource->type == SRResourceType::Buffer) {
		auto* internal_buffer = (SRBuffer_DX12*)resource->internalState;

		if (internal_buffer->srvDescriptor != SR_INVALID_DESCRIPTOR_INDEX) {
			SRDescriptorHeap_DX12_FreeIndex(dev->descriptor_heap_cbv_srv_uav, internal_buffer->srvDescriptor);
		}
		internal_buffer->allocation->Release();
	}
	else if (resource->type == SRResourceType::Texture) {
		auto* internal_texture = (SRTexture_DX12*)resource->internalState;

		if (internal_texture->rtvDescriptor != SR_INVALID_DESCRIPTOR_INDEX) {
			SRDescriptorHeap_DX12_FreeIndex(dev->descriptor_heap_rtv, internal_texture->rtvDescriptor);
		}
		if (internal_texture->srvDescriptor != SR_INVALID_DESCRIPTOR_INDEX) {
			SRDescriptorHeap_DX12_FreeIndex(dev->descriptor_heap_cbv_srv_uav, internal_texture->srvDescriptor);
		}
		if (internal_texture->dsvDescriptor != SR_INVALID_DESCRIPTOR_INDEX) {
			SRDescriptorHeap_DX12_FreeIndex(dev->descriptor_heap_dsv, internal_texture->dsvDescriptor);
		}
		if (internal_texture->dsvReadOnlyDescriptor != SR_INVALID_DESCRIPTOR_INDEX) {
			SRDescriptorHeap_DX12_FreeIndex(dev->descriptor_heap_dsv, internal_texture->dsvReadOnlyDescriptor);
		}

		internal_texture->allocation->Release();
		ZeroMemory(internal_texture, sizeof(*internal_texture));
	}
	else if (resource->type == SRResourceType::Sampler) {
		auto* internal_sampler = (SRSampler_DX12*)resource->internalState;

		if (internal_sampler->samplerDescriptor != SR_INVALID_DESCRIPTOR_INDEX) {
			SRDescriptorHeap_DX12_FreeIndex(dev->descriptor_heap_sampler, internal_sampler->samplerDescriptor);
		}
	}
	else {
		assert(false);
	}
}

void SRGFXDX12_BindPipeline(SRGFXDevice* device, const SRPipeline* pipeline, const SRCmdList* cmdList) {
	auto* internalPipeline = to_dx12_internal(pipeline);
	auto* internalCmdList = to_dx12_internal(*cmdList);

	internalCmdList->graphicsCmdList->SetPipelineState(internalPipeline->pipeline);
	internalCmdList->graphicsCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	internalCmdList->graphicsCmdList->SetGraphicsRootSignature(internalPipeline->rootSignature);
}

void SRGFXDX12_BindViewport(SRGFXDevice* device, const SRViewport* viewport, const SRCmdList* cmdList) {
	auto* internalCmdList = to_dx12_internal(*cmdList);

	D3D12_RECT scissorRect = {
		.left = static_cast<LONG>(viewport->topLeftX),
		.top = static_cast<LONG>(viewport->topLeftY),
		.right = static_cast<LONG>(viewport->topLeftX + viewport->width),
		.bottom = static_cast<LONG>(viewport->topLeftY + viewport->height)
	};

	internalCmdList->graphicsCmdList->RSSetViewports(1, reinterpret_cast<const D3D12_VIEWPORT*>(viewport));
	internalCmdList->graphicsCmdList->RSSetScissorRects(1, &scissorRect);
}

void SRGFXDX12_BindVertexBuffer(SRGFXDevice* device, const SRBuffer* buffer, const SRCmdList* cmdList) {
	assert(has_flag(buffer->info.bindFlags, SRBindFlag::VertexBuffer));
	auto* internalBuffer = to_dx12_internal(buffer);
	auto* internalCmdList = to_dx12_internal(*cmdList);

	D3D12_VERTEX_BUFFER_VIEW vertexBufferView = {
		.BufferLocation = internalBuffer->allocation->GetResource()->GetGPUVirtualAddress(),
		.SizeInBytes = static_cast<UINT>(buffer->info.size),
		.StrideInBytes = buffer->info.stride
	};

	internalCmdList->graphicsCmdList->IASetVertexBuffers(0, 1, &vertexBufferView);
}

void SRGFXDX12_BindIndexBuffer(SRGFXDevice* device, const SRBuffer* buffer, const SRCmdList* cmdList) {
	assert(has_flag(buffer->info.bindFlags, SRBindFlag::IndexBuffer));
	auto* internalBuffer = to_dx12_internal(buffer);
	auto* internalCmdList = to_dx12_internal(*cmdList);

	D3D12_INDEX_BUFFER_VIEW indexBufferView = {
		.BufferLocation = internalBuffer->allocation->GetResource()->GetGPUVirtualAddress(),
		.SizeInBytes = static_cast<UINT>(buffer->info.size),
		.Format = DXGI_FORMAT_R32_UINT
	};

	internalCmdList->graphicsCmdList->IASetIndexBuffer(&indexBufferView);
}

void SRGFXDX12_BindRootConstantBuffer(SRGFXDevice* device, const SRBuffer* buffer, const SRCmdList* cmdList) {
	assert(has_flag(buffer->info.bindFlags, SRBindFlag::ConstantBuffer));
	auto* internalBuffer = to_dx12_internal(buffer);
	auto* internalCmdList = to_dx12_internal(*cmdList);

	internalCmdList->graphicsCmdList->SetGraphicsRootConstantBufferView(
		1,
		internalBuffer->allocation->GetResource()->GetGPUVirtualAddress()
	);
}

void SRGFXDX12_PushConstants(SRGFXDevice* device, const void* data, u32 size, const SRCmdList* cmdList) {
	auto* internalCmdList = to_dx12_internal(*cmdList);
	assert(size <= 128);

	internalCmdList->graphicsCmdList->SetGraphicsRoot32BitConstants(0, size >> 2, data, 0);
}

void SRGFXDX12_Barrier(SRGFXDevice* device, const SRBarrier* barriers, u32 numBarriers, const SRCmdList* cmdList) {
	if (!barriers || numBarriers == 0) {
		return;
	}

	// TODO: Support UAV and buffer barriers
	auto* internalCmdList = to_dx12_internal(*cmdList);
	std::vector<D3D12_TEXTURE_BARRIER> dx12Barriers;
	dx12Barriers.reserve(numBarriers);

	for (u32 i = 0; i < numBarriers; ++i) {
		const SRBarrier* barrier = &barriers[i];
		auto* internalTexture = to_dx12_internal(barrier->image.texture);

		D3D12_TEXTURE_BARRIER dx12Barrier = {
			.SyncBefore = to_dx12_pipeline_stage(barrier->image.syncBefore),
			.SyncAfter = to_dx12_pipeline_stage(barrier->image.syncAfter),
			.AccessBefore = to_dx12_access_mask(barrier->image.accessBefore),
			.AccessAfter = to_dx12_access_mask(barrier->image.accessAfter),
			.LayoutBefore = to_dx12_resource_state(barrier->image.stateBefore),
			.LayoutAfter = to_dx12_resource_state(barrier->image.stateAfter),
			.pResource = internalTexture->allocation->GetResource(),
			.Subresources = { 0xffffffff, 0, 0, 0, 0, 0 },
			.Flags = D3D12_TEXTURE_BARRIER_FLAG_NONE
		};

		if (barrier->image.stateBefore == SRResourceState::Undefined) {
			dx12Barrier.Flags = D3D12_TEXTURE_BARRIER_FLAG_DISCARD;
		}
		dx12Barriers.push_back(dx12Barrier);
	}

	const D3D12_BARRIER_GROUP barrierGroup = {
		.Type = D3D12_BARRIER_TYPE_TEXTURE,
		.NumBarriers = numBarriers,
		.pTextureBarriers = dx12Barriers.data()
	};
	internalCmdList->graphicsCmdList->Barrier(1, &barrierGroup);
}

void SRGFXDX12_BeginFrame(SRGFXDevice* device, const SRSwapchain* swapchain) {
	auto* dev = (SRGFXDeviceDX12*)device->internalState;

	if (dev->frame_counter >= SR_GFX_FRAMES_IN_FLIGHT) {
		u64 needed = dev->frame_done_values[SRQueue_Universal][dev->frame_index];
		u64 current = dev->frame_fences[SRQueue_Universal]->GetCompletedValue();

		if (current < needed) {
			HR(dev->frame_fences[SRQueue_Universal]->SetEventOnCompletion(needed, nullptr));
		}
	}

	auto* internalSwapchain = to_dx12_internal(swapchain);
	dev->image_index = internalSwapchain->swapchain->GetCurrentBackBufferIndex();
}

SRCmdList SRGFXDX12_BeginCommandList(SRGFXDevice* device, SRQueue queue) {
	auto* dev = (SRGFXDeviceDX12*)device->internalState;
	auto* internalCmdList = &dev->cmd_lists[queue][dev->frame_index];

	if (internalCmdList->graphicsCmdList == nullptr) {
		// NOTE: We require ID3D12GraphicsCommandList7 to be available
		HR(dev->device->CreateCommandList(
			0,
			to_dx12_cmd_list_type(queue),
			dev->cmd_allocators[queue][dev->frame_index],
			nullptr,
			IID_PPV_ARGS(&internalCmdList->graphicsCmdList)
		));

		// All command lists begin in recording state, so we close it
		internalCmdList->graphicsCmdList->Close();
	}

	HR(dev->cmd_allocators[queue][dev->frame_index]->Reset());
	HR(internalCmdList->graphicsCmdList->Reset(
		dev->cmd_allocators[queue][dev->frame_index],
		nullptr
	));

	ID3D12DescriptorHeap* descriptor_heaps[] = {
		dev->descriptor_heap_cbv_srv_uav->heapObject,
		dev->descriptor_heap_sampler->heapObject
	};
	internalCmdList->graphicsCmdList->SetDescriptorHeaps(_countof(descriptor_heaps), descriptor_heaps);

	return SRCmdList{ internalCmdList };
}

void SRGFXDX12_BeginRenderPassSwapchain(SRGFXDevice* device, const SRSwapchain* swapchain, const SRCmdList* cmdList) {
	auto* dev = (SRGFXDeviceDX12*)device->internalState;
	auto* internalSwapchain = to_dx12_internal(swapchain);
	auto* internalCmdList = to_dx12_internal(*cmdList);

	// NOTE: Stingray always assumes that the swapchain will never be cleared,
	// and thus we assume an immediate overwrite of the swapchain backbuffer.
	// Hence why D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD is used. The
	// reason for this is that swapchain clears are notoriously slow to perform,
	// and thus it was decided to not allow such clears.
	D3D12_CLEAR_VALUE clear_value = {
		.Format = to_dx12_format(swapchain->info.format),
		.Color = { 0.0f, 0.0f, 0.0f, 1.0f }
	};

	D3D12_RENDER_PASS_RENDER_TARGET_DESC pass_rtv_desc = {
		.cpuDescriptor = SRDescriptorHeap_DX12_GetCPUHandle(
			dev->descriptor_heap_rtv,
			internalSwapchain->rtvDescriptors[dev->image_index]
		),
		.BeginningAccess = {
			.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR,
			.Clear = { .ClearValue = clear_value }
		},
		.EndingAccess = {
			.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE
		}
	};

	// TODO: Transition layout
	SRImageTransitionInfo_DX12 transition_info = {
		.image = internalSwapchain->images[dev->image_index],
		.oldLayout = D3D12_BARRIER_LAYOUT_PRESENT,
		.newLayout = D3D12_BARRIER_LAYOUT_RENDER_TARGET,
		.srcAccessMask = D3D12_BARRIER_ACCESS_NO_ACCESS,
		.dstAccessMask = D3D12_BARRIER_ACCESS_RENDER_TARGET,
		.srcStageMask = D3D12_BARRIER_SYNC_NONE,
		.dstStageMask = D3D12_BARRIER_SYNC_RENDER_TARGET
	};
	SRDX12Helpers::transition_image_layout(transition_info, internalCmdList->graphicsCmdList);

	internalCmdList->graphicsCmdList->BeginRenderPass(
		1U,
		&pass_rtv_desc,
		nullptr,
		D3D12_RENDER_PASS_FLAG_NONE
	);
}

void SRGFXDX12_BeginRenderPass(SRGFXDevice* device, const SRPassInfo* passInfo, const SRCmdList* cmdList) {
	auto* dev = (SRGFXDeviceDX12*)device->internalState;
	auto* internalCmdList = to_dx12_internal(*cmdList);

	// RTVs
	std::vector<D3D12_RENDER_PASS_RENDER_TARGET_DESC> passRTVDescs;
	D3D12_RENDER_PASS_DEPTH_STENCIL_DESC passDSVDesc = {};
	passRTVDescs.reserve(passInfo->numColorAttachments);

	for (u32 i = 0; i < passInfo->numColorAttachments; ++i) {
		const SRPassInfo::Attachment* attachment = &passInfo->colorAttachments[i];
		auto* internalTexture = to_dx12_internal(attachment->texture);
		assert(internalTexture);

		D3D12_RENDER_PASS_RENDER_TARGET_DESC rtvDesc = {
			.cpuDescriptor = SRDescriptorHeap_DX12_GetCPUHandle(
				dev->descriptor_heap_rtv,
				internalTexture->rtvDescriptor
			)
		};

		if (attachment->loadOp == SRLoadOp::Clear) {
			rtvDesc.BeginningAccess.Clear.ClearValue = {
				.Format = to_dx12_format(attachment->texture->info.format),
				.Color = { 0.0f, 0.0f, 0.0f, 1.0f }
			};
		}

		rtvDesc.BeginningAccess.Type = to_dx12_load_op(attachment->loadOp);
		rtvDesc.EndingAccess.Type = to_dx12_store_op(attachment->storeOp);

		passRTVDescs.push_back(rtvDesc);
	}

	// DSV
	bool hasDepthAttachment = passInfo->depthAttachment.texture != nullptr;
	bool isReadOnlyDepth = false;
	if (hasDepthAttachment) {
		const SRPassInfo::Attachment& depthAttachment = passInfo->depthAttachment;
		auto internalTexture = to_dx12_internal(depthAttachment.texture);
		assert(internalTexture);

		if (depthAttachment.loadOp == SRLoadOp::Clear) {
			passDSVDesc.cpuDescriptor = SRDescriptorHeap_DX12_GetCPUHandle(
				dev->descriptor_heap_dsv,
				internalTexture->dsvDescriptor
			);
			passDSVDesc.DepthBeginningAccess.Clear.ClearValue = {
				.Format = to_dx12_format(depthAttachment.texture->info.format),
				.DepthStencil = {
					.Depth = depthAttachment.clearValue,
					.Stencil = 0
				}
			};
		}
		else if (depthAttachment.loadOp == SRLoadOp::Load) {
			passDSVDesc.cpuDescriptor = SRDescriptorHeap_DX12_GetCPUHandle(
				dev->descriptor_heap_dsv,
				internalTexture->dsvReadOnlyDescriptor
			);
			isReadOnlyDepth = true;
		}
		else {
			assert(false); // INVALID
		}

		passDSVDesc.DepthBeginningAccess.Type = to_dx12_load_op(depthAttachment.loadOp);
		passDSVDesc.DepthEndingAccess.Type = to_dx12_store_op(depthAttachment.storeOp);
		passDSVDesc.StencilBeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS;
		passDSVDesc.StencilEndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS;
	}

	internalCmdList->graphicsCmdList->BeginRenderPass(
		passInfo->numColorAttachments,
		passRTVDescs.data(),
		hasDepthAttachment ? &passDSVDesc : nullptr,
		isReadOnlyDepth ? D3D12_RENDER_PASS_FLAG_BIND_READ_ONLY_DEPTH : D3D12_RENDER_PASS_FLAG_NONE
	);
}

void SRGFXDX12_EndRenderPassSwapchain(SRGFXDevice* device, const SRSwapchain* swapchain, const SRCmdList* cmdList) {
	auto* dev = (SRGFXDeviceDX12*)device->internalState;
	auto* internalSwapchain = to_dx12_internal(swapchain);
	auto* internalCmdList = to_dx12_internal(*cmdList);
	internalCmdList->graphicsCmdList->EndRenderPass();

	SRImageTransitionInfo_DX12 transitionInfo = {
		.image = internalSwapchain->images[dev->image_index],
		.oldLayout = D3D12_BARRIER_LAYOUT_RENDER_TARGET,
		.newLayout = D3D12_BARRIER_LAYOUT_PRESENT,
		.srcAccessMask = D3D12_BARRIER_ACCESS_RENDER_TARGET,
		.dstAccessMask = D3D12_BARRIER_ACCESS_NO_ACCESS,
		.srcStageMask = D3D12_BARRIER_SYNC_RENDER_TARGET,
		.dstStageMask = D3D12_BARRIER_SYNC_NONE,
	};
	SRDX12Helpers::transition_image_layout(transitionInfo, internalCmdList->graphicsCmdList);
}

void SRGFXDX12_EndRenderPass(SRGFXDevice* device, const SRCmdList* cmdList) {
	auto* dev = (SRGFXDeviceDX12*)device->internalState;
	auto* internalCmdList = to_dx12_internal(*cmdList);
	internalCmdList->graphicsCmdList->EndRenderPass();
}

void SRGFXDX12_SubmitCommandLists(SRGFXDevice* device, const SRSwapchain* swapchain) {
	auto* dev = (SRGFXDeviceDX12*)device->internalState;
	auto* internalSwapchain = to_dx12_internal(swapchain);

	dev->cmd_lists[SRQueue_Universal][dev->frame_index].graphicsCmdList->Close();
	ID3D12CommandList* cmd_lists[] = {
		dev->cmd_lists[SRQueue_Universal][dev->frame_index].graphicsCmdList
	};

	dev->cmd_queues[SRQueue_Universal]->ExecuteCommandLists(_countof(cmd_lists), cmd_lists);

	u64 signal_value = dev->frame_counter + 1;
	HR(dev->cmd_queues[SRQueue_Universal]->Signal(dev->frame_fences[SRQueue_Universal], signal_value));

	UINT syncInterval = swapchain->info.vSync ? 1 : 0;
	UINT presentFlags = dev->is_tearing_supported && !swapchain->info.vSync ? DXGI_PRESENT_ALLOW_TEARING : 0;
	internalSwapchain->swapchain->Present(syncInterval, presentFlags);

	dev->frame_done_values[SRQueue_Universal][dev->frame_index] = signal_value;
	dev->frame_index = (dev->frame_index + 1) % SR_GFX_FRAMES_IN_FLIGHT;
	++dev->frame_counter;
}

void SRGFXDX12_Draw(SRGFXDevice* device, u32 vtxCount, u32 startVtx, const SRCmdList* cmdList) {
	auto* internalCmdList = to_dx12_internal(*cmdList);
	internalCmdList->graphicsCmdList->DrawInstanced(vtxCount, 1, startVtx, 0);
}

void SRGFXDX12_DrawIndexed(SRGFXDevice* device, u32 idxCount, u32 startIdx, u32 baseVtx, const SRCmdList* cmdList) {
	auto* internalCmdList = to_dx12_internal(*cmdList);
	internalCmdList->graphicsCmdList->DrawIndexedInstanced(
		idxCount,
		1,
		startIdx,
		baseVtx,
		0
	);
}

void SRGFXDX12_DrawInstanced(SRGFXDevice* device, u32 vtx_count, u32 inst_count, u32 start_vtx, uint32_t start_inst, const SRCmdList* cmd_list) {
	auto* internal_cmd_list = to_dx12_internal(*cmd_list);
	internal_cmd_list->graphicsCmdList->DrawInstanced(
		vtx_count,
		inst_count,
		start_vtx,
		start_inst
	);
}

void SRGFXDX12_DispatchMesh(SRGFXDevice* device, u32 x, u32 y, u32 z, const SRCmdList* cmdList) {
	auto* internalCmdList = to_dx12_internal(*cmdList);
	internalCmdList->graphicsCmdList->DispatchMesh(x, y, z);
}

SRDescriptorIndex SRGFXDX12_GetDescriptorIndexSRV(SRGFXDevice* device, const SRResource* resource) {
	if (resource->type == SRResourceType::Texture) {
		auto* internalTexture = (SRTexture_DX12*)resource->internalState;
		return internalTexture->srvDescriptor;
	}
	if (resource->type == SRResourceType::Buffer) {
		auto* internalBuffer = (SRBuffer_DX12*)resource->internalState;
		return internalBuffer->srvDescriptor;
	}

	assert(false);
	return SR_INVALID_DESCRIPTOR_INDEX;
}

SRShaderCompileTarget SRGFXDX12_GetShaderCompileTarget(SRGFXDevice* device) {
	return SRShaderCompileTarget::DXIL;
}

void SRGFXDX12_WaitForGPU(SRGFXDevice* device) {
	auto* dev = (SRGFXDeviceDX12*)device->internalState;

	// TODO: Right now we only use universal queue, remember that when we add
	// dedicated compute/copy queue we also need to Signal those here.
	ID3D12Fence* tempFence;
	HR(dev->device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&tempFence)));
	HR(dev->cmd_queues[SRQueue_Universal]->Signal(tempFence, 1));

	if (tempFence->GetCompletedValue() < 1) {
		HR(tempFence->SetEventOnCompletion(1, nullptr));
	}
	tempFence->Release();
}

void SRGFXDX12_FlushInitialUploads(SRGFXDevice* device) {
	auto* dev = (SRGFXDeviceDX12*)device->internalState;
	HR(dev->upload_cmd_list->Close());

	ID3D12CommandList* cmdLists[1] = { dev->upload_cmd_list };
	dev->cmd_queues[SRQueue_Copy]->ExecuteCommandLists(1, cmdLists );

	// TEMPORARY
	ID3D12Fence* tempFence;
	HR(dev->device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&tempFence)));
	HR(dev->cmd_queues[SRQueue_Copy]->Signal(tempFence, 1));

	if (tempFence->GetCompletedValue() < 1) {
		HR(tempFence->SetEventOnCompletion(1, nullptr));
	}
	tempFence->Release();

	// Release everything at this point
	uintptr_t upload_bytes = (uintptr_t)(dev->arena_upload->allocated - dev->arena_upload->data);
	u64 upload_count = upload_bytes / sizeof(D3D12MA::Allocation*);
	D3D12MA::Allocation** uploads = (D3D12MA::Allocation**)dev->arena_upload->data;

	for (u64 i = 0; i < upload_count; ++i) {
		D3D12MA::Allocation* upload = uploads[i];
		upload->Release();
	}

	SRArena_Clear(dev->arena_upload);
	dev->upload_cmd_allocator->Reset();
}

void SRGFXDX12_SetupImGuiInitInfo(SRGFXDevice* device, SRFormat swapchainFormat) {
	auto* dev = (SRGFXDeviceDX12*)device->internalState;

	ImGui_ImplDX12_InitInfo init_info = {};
	init_info.Device = dev->device;
	init_info.CommandQueue = dev->cmd_queues[SRQueue_Universal];
	init_info.NumFramesInFlight = SR_GFX_FRAMES_IN_FLIGHT;
	init_info.RTVFormat = to_dx12_format(swapchainFormat);
	init_info.UserData = dev->descriptor_heap_cbv_srv_uav;
	init_info.SrvDescriptorHeap = dev->descriptor_heap_cbv_srv_uav->heapObject;

	init_info.SrvDescriptorAllocFn = [](
		ImGui_ImplDX12_InitInfo* init_info,
		D3D12_CPU_DESCRIPTOR_HANDLE* cpu_handle,
		D3D12_GPU_DESCRIPTOR_HANDLE* gpu_handle
	) {
		SRDescriptorHeap_DX12* heap = (SRDescriptorHeap_DX12*)init_info->UserData;
		SRDescriptorIndex index = SRDescriptorHeap_DX12_GetNextIndex(heap);
		*cpu_handle = SRDescriptorHeap_DX12_GetCPUHandle(heap, index);
		*gpu_handle = SRDescriptorHeap_DX12_GetGPUHandle(heap, index);
	};

	init_info.SrvDescriptorFreeFn = [](
		ImGui_ImplDX12_InitInfo* init_info,
		D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle,
		D3D12_GPU_DESCRIPTOR_HANDLE
	) {
		SRDescriptorHeap_DX12* heap = (SRDescriptorHeap_DX12*)init_info->UserData;

		// NOTE: CPU and GPU handle are related, freeing CPU also frees GPU
		SRDescriptorIndex index = SRDescriptorHeap_DX12_GetIndexFromCPUHandle(heap, cpu_handle);
		SRDescriptorHeap_DX12_FreeIndex(heap, index);
	};

	ImGui_ImplDX12_Init(&init_info);
}
