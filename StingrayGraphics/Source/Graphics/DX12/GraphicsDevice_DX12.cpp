#include "GraphicsDevice_DX12.h"
#include "Graphics/DX12/GraphicsHelpers_DX12.h"
#include "Graphics/DX12/GraphicsTypes_DX12.h"
#include "Core/Logger.h"
#include "Utilities/TextUtilities.h"

#include <imgui.h>
#include <imgui_impl_dx12.h>

#include "d3d12.h"
#include "d3dx12/d3dx12_pipeline_state_stream.h"
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <wrl/client.h>
#include <Windows.h>

#include <stdlib.h>

using namespace Microsoft::WRL;
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
	.dispatch_mesh               = SRGFXDX12_DispatchMesh,
	.get_descriptor_index_srv    = SRGFXDX12_GetDescriptorIndexSRV,
	.get_shader_platform_info    = SRGFXDX12_GetShaderPlatformInfo,
	.wait_for_gpu                = SRGFXDX12_WaitForGPU,
	.flush_initial_uploads       = SRGFXDX12_FlushInitialUploads,
	.setup_imgui_init_info       = SRGFXDX12_SetupImGuiInitInfo
};

struct SRGFXDeviceDX12 {
	const SRWindow* m_Window;
	std::vector<std::shared_ptr<void>> m_PendingUploadResources;

	#ifdef _DEBUG
		ComPtr<ID3D12Debug> m_DebugInterface;
		ComPtr<IDXGIInfoQueue> m_DXGIDebugInfoQueue;
		ComPtr<ID3D12DebugDevice> m_DebugDevice;
	#endif

	ComPtr<ID3D12CommandAllocator> m_UploadCmdAllocator;
	ComPtr<ID3D12GraphicsCommandList7> m_UploadCmdList;
	bool m_IsUploadCmdListRecording = false;

	IDXGIFactory2* m_DXGIFactory;
	ComPtr<IDXGIAdapter1> m_Adapter;
	D3D12MA::Allocator* m_Allocator = nullptr;
	ComPtr<ID3D12Device10> m_Device;
	SRDeviceCapabilities_DX12 m_DeviceCapabilities = {};
	ComPtr<ID3D12CommandAllocator> m_CommandAllocators[SRQueue_COUNT][SR_GFX_FRAMES_IN_FLIGHT];
	ID3D12CommandQueue* m_CommandQueues[SRQueue_COUNT];

	SRDescriptorHeap_DX12 m_ResourceDescriptorHeap = { D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, MAX_RESOURCE_DESCRIPTORS }; // CBV + SRV + UAV
	SRDescriptorHeap_DX12 m_SamplerDescriptorHeap = { D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, MAX_SAMPLER_DESCRIPTORS };
	SRDescriptorHeap_DX12 m_RTVDescriptorHeap = { D3D12_DESCRIPTOR_HEAP_TYPE_RTV, MAX_RTV_DESCRIPTORS };
	SRDescriptorHeap_DX12 m_DSVDescriptorHeap = { D3D12_DESCRIPTOR_HEAP_TYPE_DSV, MAX_DSV_DESCRIPTORS };

	ID3D12Fence* m_FrameFences[SRQueue_COUNT];
	u64 m_FrameDoneValues[SRQueue_COUNT][SR_GFX_FRAMES_IN_FLIGHT] = {};
	u64 m_NextGPUSignalValue = 1;

	std::vector<std::unique_ptr<SRCmdList_DX12>> m_PerFrameCmdLists[SR_GFX_FRAMES_IN_FLIGHT];
	size_t m_PerFrameCmdListCounters[SR_GFX_FRAMES_IN_FLIGHT] = {};
	u32 m_FrameIndex = 0;
	u32 m_ImageIndex = 0;
	u64 m_FrameCounter = 0;
	bool m_IsTearingSupported = false;

	static constexpr SRShaderPlatformInfo m_ShaderPlatformInfo = {
		SRShaderCompileTarget::DXIL,
		"sm_6_6"
	};
	static constexpr u32 MAX_RESOURCE_DESCRIPTORS = 32768;
	static constexpr u32 MAX_SAMPLER_DESCRIPTORS = 16;
	static constexpr u32 MAX_RTV_DESCRIPTORS = 256;
	static constexpr u32 MAX_DSV_DESCRIPTORS = 32;
};

internal void SRGFXDeviceDX12_CreateDebugInterface(SRGFXDeviceDX12* dev) {
#ifdef _DEBUG
	if (FAILED(D3D12GetDebugInterface(IID_PPV_ARGS(&dev->m_DebugInterface)))) {
		SRLOG_WARN_CAT(SRLOG_CAT_DX12, "Failed to create base ID3D12Debug interface. Debug information will be limited");
		return;
	}

	dev->m_DebugInterface->EnableDebugLayer();

	ComPtr<ID3D12Debug1> debugInterface1;
	if (FAILED(dev->m_DebugInterface.As(&debugInterface1))) {
		SRLOG_WARN_CAT(SRLOG_CAT_DX12, "Failed to create ID3D12Debug1 interface. GBV/SCQV information will not be available");
		return;
	}

	debugInterface1->SetEnableGPUBasedValidation(TRUE);
	debugInterface1->SetEnableSynchronizedCommandQueueValidation(TRUE);
#else
	return;
#endif
}

internal void SRGFXDeviceDX12_CreateDXGIDebugInterface(SRGFXDeviceDX12* dev) {
#ifdef _DEBUG
	if (FAILED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dev->m_DXGIDebugInfoQueue)))) {
		SRLOG_WARN_CAT(SRLOG_CAT_DX12, "Failed to create DXGI debug interface. DXGI-related information will not be available");
		return;
	}

	SR_DX12_CHECK(
		dev->m_DXGIDebugInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, TRUE),
		"Set break on severity"
	);
	SR_DX12_CHECK(
		dev->m_DXGIDebugInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, TRUE),
		"Set break on severity"
	);
#else
	return;
#endif
}

internal void SRGFXDeviceDX12_CreateDXGIFactory(SRGFXDeviceDX12* dev) {
	UINT dxgiFactoryFlags = 0;
#ifdef _DEBUG
	dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

	SR_DX12_CHECK(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&dev->m_DXGIFactory)), "DXGI factory creation");
	SRLOG_DEBUG_CAT(SRLOG_CAT_DX12, "Successfully created DXGI factory");

	IDXGIFactory5* dxgiFactory5 = nullptr;
	if (SUCCEEDED(dev->m_DXGIFactory->QueryInterface(IID_PPV_ARGS(&dxgiFactory5)))) {
		BOOL allowTearing = FALSE;
		HRESULT hr = dxgiFactory5->CheckFeatureSupport(
			DXGI_FEATURE_PRESENT_ALLOW_TEARING,
			&allowTearing,
			sizeof(allowTearing)
		);
		dxgiFactory5->Release();

		dev->m_IsTearingSupported = SUCCEEDED(hr) && allowTearing == TRUE;
	}
}

internal void SRGFXDeviceDX12_CreateDevice(SRGFXDeviceDX12* dev) {
	u32 pickedDeviceIdx = ~0U;
	std::string deviceName;

	// NOTE: We prefer IDXGIFactory6 since it allows us to enumerate adapters
	// based on GPU preference. If it's not available, we pick a device with
	// EnumAdapters1 instead.
	IDXGIFactory6* dxgiFactory6 = nullptr;
	bool isDXGIFactory6Available = SUCCEEDED(dev->m_DXGIFactory->QueryInterface(IID_PPV_ARGS(&dxgiFactory6)));

	for (UINT i = 0;; ++i) {
		ComPtr<IDXGIAdapter1> adapter;
		HRESULT hr;

		if (isDXGIFactory6Available) {
			hr = dxgiFactory6->EnumAdapterByGpuPreference(
				i,
				DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
				IID_PPV_ARGS(&adapter)
			);
			dxgiFactory6->Release();
		}
		else {
			hr = dev->m_DXGIFactory->EnumAdapters1(i, adapter.GetAddressOf());
		}

		if (FAILED(hr)) {
			break;
		}

		DXGI_ADAPTER_DESC1 adapterDesc;
		adapter->GetDesc1(&adapterDesc);
		deviceName = SRTextUtilities::to_string(adapterDesc.Description);

		if (adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
			SRLOG_DEBUG_CAT(SRLOG_CAT_DX12, "[GPU%u] %s REJECTED. Software/WARP adapter", i, deviceName.c_str());
			continue;
		}

		ComPtr<ID3D12Device> device;
		hr = D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device));
		if (FAILED(hr)) {
			SRLOG_DEBUG_CAT(SRLOG_CAT_DX12, "[GPU%u] %s REJECTED. Device creation failed", i, deviceName.c_str());
			continue;
		}

		auto capabilities = SRDX12Helpers::query_device_capabilities(device.Get());

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
			SRLOG_WARN_CAT(SRLOG_CAT_DX12, "[GPU%u] %s REJECTED. Missing %zu requirement(s):", i, deviceName.c_str(), missing.size());

			for (const auto& str : missing) {
				SRLOG_WARN_CAT(SRLOG_CAT_DX12, "\t%s", str.c_str());
			}

			continue;
		}

		SR_DX12_CHECK(device.As(&dev->m_Device), "Create as ID3D12Device10");
		dev->m_Adapter = adapter;

#ifdef _DEBUG
		if (FAILED(dev->m_Device.As(&dev->m_DebugDevice))) {
			SRLOG_DEBUG_CAT(SRLOG_CAT_DX12, "ID3D12DebugDevice not available. D3D12 object reporting disabled");
		}
#endif

		dev->m_DeviceCapabilities = capabilities;
		pickedDeviceIdx = i;
		break;
	}

	if (pickedDeviceIdx == ~0) {
		SRLOG_CRITICAL_CAT(SRLOG_CAT_DX12, "No suitable GPU found");
		throw std::runtime_error("DX12 ERROR: No suitable GPU found");
	}

	SRLOG_INFO_CAT(SRLOG_CAT_DX12, "Picked [GPU%u] %s", pickedDeviceIdx, deviceName.c_str());
}

internal void SRGFXDeviceDX12_CreateMemoryAllocator(SRGFXDeviceDX12* dev) {
	D3D12MA::ALLOCATOR_DESC allocatorDesc = {
		.Flags = D3D12MA_RECOMMENDED_ALLOCATOR_FLAGS,
		.pDevice = dev->m_Device.Get(),
		.pAdapter = dev->m_Adapter.Get()
	};

	SR_DX12_CHECK(D3D12MA::CreateAllocator(&allocatorDesc, &dev->m_Allocator), "Create D3D12 Memory Allocator");
}

internal void SRGFXDeviceDX12_CreateCommandAllocators(SRGFXDeviceDX12* dev) {
	for (u32 f = 0; f < SR_GFX_FRAMES_IN_FLIGHT; ++f) {
		// Universal command allocators (direct)
		SR_DX12_CHECK(dev->m_Device->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(&dev->m_CommandAllocators[SRQueue_Universal][f])
		), "Command allocator creation");

		// TODO: Create command allocators for other queue types too. We will need
		// it eventually.

		// Compute (TODO)

		// Copy command allocators (TODO)
	}

	// TEMPORARY
	SR_DX12_CHECK(dev->m_Device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_COPY,
		IID_PPV_ARGS(&dev->m_UploadCmdAllocator)
	), "Upload command allocator creation");

	SR_DX12_CHECK(dev->m_Device->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_COPY,
		dev->m_UploadCmdAllocator.Get(),
		nullptr,
		IID_PPV_ARGS(&dev->m_UploadCmdList)
	), "Command list creation");
	dev->m_UploadCmdList->Close();
}

internal void SRGFXDeviceDX12_CreateCommandQueues(SRGFXDeviceDX12* dev) {
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc = {
		.Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
		.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
		.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
		.NodeMask = 0
	};

	SR_DX12_CHECK(dev->m_Device->CreateCommandQueue(
		&commandQueueDesc,
		IID_PPV_ARGS(&dev->m_CommandQueues[SRQueue_Universal])
	), "Command queue creation");

	commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;

	SR_DX12_CHECK(dev->m_Device->CreateCommandQueue(
		&commandQueueDesc,
		IID_PPV_ARGS(&dev->m_CommandQueues[SRQueue_Copy])
	), "Command queue creation");

	// TODO: Create command queues for other queue types too. We will need it
	// eventually.
}

void SRGFXDeviceDX12_CreateSyncObjects(SRGFXDeviceDX12* dev) {
	for (u32 q = 0; q < SRQueue_COUNT; ++q) {
		SR_DX12_CHECK(dev->m_Device->CreateFence(
			0,
			D3D12_FENCE_FLAG_NONE,
			IID_PPV_ARGS(&dev->m_FrameFences[q])
		), "Create frame fence");
	}
}

internal void SRGFXDeviceDX12_CreateDescriptorHeaps(SRGFXDeviceDX12* dev) {
	dev->m_ResourceDescriptorHeap.initialize(dev->m_Device.Get());
	dev->m_SamplerDescriptorHeap.initialize(dev->m_Device.Get());
	dev->m_RTVDescriptorHeap.initialize(dev->m_Device.Get());
	dev->m_DSVDescriptorHeap.initialize(dev->m_Device.Get());
}

// ------------------------------ Public API ------------------------------
void SRGFXDX12_CreateDevice(const SRWindow* window, SRGFXDevice* device) {
	//SRGFXDeviceDX12* devDX12 = (SRGFXDeviceDX12*)malloc(sizeof(SRGFXDeviceDX12));
	SRGFXDeviceDX12* devDX12 = new SRGFXDeviceDX12();
	//ZeroMemory(devDX12, sizeof(*devDX12));
	devDX12->m_Window = window;

	device->internalState = devDX12;
	device->vtbl = &SRGFXDevice_DX12_VTable;

	SRGFXDeviceDX12_CreateDebugInterface(devDX12);
	SRGFXDeviceDX12_CreateDXGIDebugInterface(devDX12);
	SRGFXDeviceDX12_CreateDXGIFactory(devDX12);
	SRGFXDeviceDX12_CreateDevice(devDX12);
	SRGFXDeviceDX12_CreateMemoryAllocator(devDX12);
	SRGFXDeviceDX12_CreateCommandAllocators(devDX12);
	SRGFXDeviceDX12_CreateCommandQueues(devDX12);
	SRGFXDeviceDX12_CreateSyncObjects(devDX12);
	SRGFXDeviceDX12_CreateDescriptorHeaps(devDX12);
}

void SRGFXDX12_DestroyDevice(SRGFXDevice* device) {
	SRGFXDeviceDX12* dev = (SRGFXDeviceDX12*)device->internalState;

	for (u32 q = 0; q < SRQueue_COUNT; ++q) {
		dev->m_FrameFences[q]->Release();
	}

	dev->m_CommandQueues[SRQueue_Universal]->Release();
	dev->m_CommandQueues[SRQueue_Copy]->Release();

	dev->m_DXGIFactory->Release();

	dev->m_Allocator->Release();
	dev->m_Allocator = nullptr;

	free(device->internalState);
	device->internalState = nullptr;
}

u32 SRGFXDX12_GetFrameIndex(SRGFXDevice* device) {
	auto* dev = (SRGFXDeviceDX12*)device->internalState;
	return dev->m_FrameIndex;
}

void SRGFXDX12_CreateSwapchain(SRGFXDevice* device, const SRSwapchainInfo* info, SRSwapchain* swapchain) {
	auto* dev = (SRGFXDeviceDX12*)device->internalState;
	auto internalSwapchain = std::make_shared<SRSwapchain_DX12>();
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
		.Flags = dev->m_IsTearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0U
	};

	HWND windowHandle = (HWND)dev->m_Window->get_internal_handle();
	ComPtr<IDXGISwapChain1> dxgiSwapchain1;
	SR_DX12_CHECK(dev->m_DXGIFactory->CreateSwapChainForHwnd(
		dev->m_CommandQueues[SRQueue_Universal],
		windowHandle,
		&swapchainDesc,
		nullptr,
		nullptr,
		dxgiSwapchain1.GetAddressOf()
	), "Swapchain creation");
	SR_DX12_CHECK(dxgiSwapchain1.As(&internalSwapchain->swapchain), "Convert IDXGISwapchain1 to IDXGISwapchain3");
	SR_DX12_CHECK(dev->m_DXGIFactory->MakeWindowAssociation(windowHandle, DXGI_MWA_NO_ALT_ENTER), "Disable Alt+Enter");

	internalSwapchain->images.resize(info->numBuffers);
	internalSwapchain->rtvDescriptors.reserve(info->numBuffers);

	for (u32 i = 0; i < info->numBuffers; ++i) {
		SR_DX12_CHECK(internalSwapchain->swapchain->GetBuffer(
			i,
			IID_PPV_ARGS(&internalSwapchain->images[i])
		), "Get backbuffer");

		const SRDescriptorIndex rtvIndex = dev->m_RTVDescriptorHeap.get_next_index();
		dev->m_Device->CreateRenderTargetView(
			internalSwapchain->images[i].Get(),
			nullptr,
			dev->m_RTVDescriptorHeap.get_cpu_handle(rtvIndex)
		);
		internalSwapchain->rtvDescriptors.push_back(rtvIndex);
	}
}

void SRGFXDX12_CreatePipeline(SRGFXDevice* device, const SRPipelineInfo* info, SRPipeline* pipeline) {
	auto* dev = (SRGFXDeviceDX12*)device->internalState;
	auto internalPipeline = std::make_shared<SRPipeline_DX12>();
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

	D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc {
		.Version = D3D_ROOT_SIGNATURE_VERSION_1_1,
		.Desc_1_1 = {
			.NumParameters = static_cast<UINT>(std::size(rootParameters)),
			.pParameters = rootParameters,
			.NumStaticSamplers = 0,
			.pStaticSamplers = nullptr,
			// TODO: Improve this logic
			.Flags = (
				D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED |
				D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED
			)
		}
	};

	if (!info->meshShader) {
		rootSignatureDesc.Desc_1_1.Flags |= D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	}

	ComPtr<ID3DBlob> rootSignatureBlob = nullptr;
	ComPtr<ID3DBlob> rootSignatureErrorBlob = nullptr;
	SR_DX12_CHECK(D3D12SerializeVersionedRootSignature(
		&rootSignatureDesc,
		&rootSignatureBlob,
		&rootSignatureErrorBlob
	), "Serialize versioned root signature");

	SR_DX12_CHECK(dev->m_Device->CreateRootSignature(
		0U,
		rootSignatureBlob->GetBufferPointer(),
		rootSignatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&internalPipeline->rootSignature)
	), "Create root signature");
	psoStream.rootSignature = internalPipeline->rootSignature.Get();

	if (info->vertexShader != nullptr) {
		psoStream.vertexShader = { info->vertexShader->byteCode.data(), info->vertexShader->byteCode.size(), };
	}
	if (info->pixelShader != nullptr) {
		psoStream.pixelShader = { info->pixelShader->byteCode.data(), info->pixelShader->byteCode.size() };
	}
	if (info->meshShader != nullptr) {
		psoStream.meshShader = { info->meshShader->byteCode.data(), info->meshShader->byteCode.size() };
	}
	if (info->taskShader != nullptr) {
		psoStream.taskShader = { info->taskShader->byteCode.data(), info->taskShader->byteCode.size() };
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
	inputLayoutDesc.NumElements = static_cast<UINT>(info->inputLayout.elements.size());
	std::vector<D3D12_INPUT_ELEMENT_DESC> inputElements;
	inputElements.reserve(info->inputLayout.elements.size());

	for (size_t i = 0; i < info->inputLayout.elements.size(); ++i) {
		const auto& element = info->inputLayout.elements[i];
		D3D12_INPUT_ELEMENT_DESC dx12Element = {
			.SemanticName = element.name.c_str(),
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

	SR_DX12_CHECK(dev->m_Device->CreatePipelineState(
		&psoStreamDesc,
		IID_PPV_ARGS(&internalPipeline->pipeline)
	), "Create pipeline state");
}

void SRGFXDX12_CreateBuffer(SRGFXDevice* device, const SRBufferInfo* info, SRBuffer* buffer, const void* data) {
	auto* dev = (SRGFXDeviceDX12*)device->internalState;
	auto internalBuffer = std::make_shared<SRBuffer_DX12>();

	buffer->type = SRResourceType::Buffer;
	buffer->info = *info;
	buffer->internalState = internalBuffer;
	buffer->mappedData = nullptr;
	buffer->mappedSize = 0;

	D3D12_RESOURCE_DESC1 resourceDesc = {
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
		resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	}
	// TODO: Look into whether or not this is always correct
	if (!has_flag(info->bindFlags, SRBindFlag::ShaderResource)) {
		resourceDesc.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
	}

	D3D12MA::ALLOCATION_DESC allocDesc = {
		.HeapType = D3D12_HEAP_TYPE_DEFAULT,
	};

	switch (info->usage) {
	case SRUsage::Default:
		allocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
		break;
	case SRUsage::Upload:
		allocDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
		break;
	}
	SR_DX12_CHECK(dev->m_Allocator->CreateResource3(
		&allocDesc,
		&resourceDesc,
		D3D12_BARRIER_LAYOUT_UNDEFINED,
		nullptr,
		0,
		nullptr,
		&internalBuffer->allocation,
		IID_NULL, nullptr
	), "CreateResource3");

	if (info->usage == SRUsage::Default && data != nullptr) {
		// Staging buffer
		SRBufferInfo stagingBufferInfo = *info;
		stagingBufferInfo.usage = SRUsage::Upload;
		stagingBufferInfo.bindFlags = SRBindFlag::None;
		stagingBufferInfo.miscFlags = SRMiscFlag::None;

		SRBuffer stagingBuffer;
		SRGFXDX12_CreateBuffer(device, &stagingBufferInfo, &stagingBuffer, data);

		dev->m_PendingUploadResources.push_back(stagingBuffer.internalState);
		auto internalStagingBuffer = to_dx12_internal(stagingBuffer);

		// Copy staging buffer into target buffer
		if (!dev->m_IsUploadCmdListRecording) {
			SR_DX12_CHECK(dev->m_UploadCmdAllocator->Reset(), "Reset command allocator");
			SR_DX12_CHECK(dev->m_UploadCmdList->Reset(
				dev->m_UploadCmdAllocator.Get(),
				nullptr
			), "Begin upload command list recording");

			dev->m_IsUploadCmdListRecording = true;
		}

		ID3D12Resource* srcResource = internalStagingBuffer->allocation->GetResource();
		ID3D12Resource* dstResource = internalBuffer->allocation->GetResource();
		dev->m_UploadCmdList->CopyResource(dstResource, srcResource);
	}
	else if (info->usage == SRUsage::Upload) {
		internalBuffer->allocation->GetResource()->Map(0, nullptr, &buffer->mappedData);
		buffer->mappedSize = info->size;

		if (data != nullptr) {
			memcpy(buffer->mappedData, data, info->size);
		}

		// NOTE: We always perform persistent mappings, so no unmap is necessary
	}

	// Descriptors
	// TODO: UAV
	if (has_flag(info->bindFlags, SRBindFlag::ShaderResource)) {
		if (has_flag(info->miscFlags, SRMiscFlag::StructuredBuffer)) {
			const D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {
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

			internalBuffer->srvDescriptor = SRDX12Helpers::init_srv_descriptor(
				dev->m_Device.Get(),
				internalBuffer->allocation->GetResource(),
				srvDesc,
				dev->m_ResourceDescriptorHeap
			);
		}
	}
	// TODO: CBV
}

void SRGFXDX12_CreateTexture(SRGFXDevice* device, const SRTextureInfo* info, SRTexture* texture, const SRSubresourceData* data) {
	assert(info->usage == SRUsage::Default);

	auto* dev = (SRGFXDeviceDX12*)device->internalState;
	auto internalTexture = std::make_shared<SRTexture_DX12>();

	texture->info = *info;
	texture->internalState = internalTexture;
	texture->type = SRResourceType::Texture;

	D3D12_RESOURCE_DESC1 resourceDesc = {
		.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
		.Alignment = 0,
		.Width = static_cast<UINT64>(info->width),
		.Height = info->height,
		.DepthOrArraySize = static_cast<UINT16>(info->depth),
		.MipLevels = static_cast<UINT16>(info->mipLevels),
		.Format = to_dx12_format(info->format),
		.SampleDesc = {.Count = info->sampleCount, .Quality = 0 },
		.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
		.Flags = D3D12_RESOURCE_FLAG_NONE
	};

	// Bind flags
	if (has_flag(info->bindFlags, SRBindFlag::DepthStencil)) {
		resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	}
	if (has_flag(info->bindFlags, SRBindFlag::UnorderedAccess)) {
		resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	}
	if (has_flag(info->bindFlags, SRBindFlag::RenderTarget)) {
		resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	}

	D3D12MA::ALLOCATION_DESC allocDesc = {
		.HeapType = D3D12_HEAP_TYPE_DEFAULT,
	};
	SR_DX12_CHECK(dev->m_Allocator->CreateResource3(
		&allocDesc,
		&resourceDesc,
		D3D12_BARRIER_LAYOUT_UNDEFINED,
		nullptr,
		0,
		nullptr,
		&internalTexture->allocation,
		IID_NULL, nullptr
	), "CreateResource3");

	// TODO: Implement subresource data copying
	if (data && data->data) {

	}

	// RTV Descriptor
	if (has_flag(info->bindFlags, SRBindFlag::RenderTarget)) {
		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {
			.Format = resourceDesc.Format,
			.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D
		};

		internalTexture->rtvDescriptor = SRDX12Helpers::init_rtv_descriptor(
			dev->m_Device.Get(),
			internalTexture->allocation->GetResource(),
			rtvDesc,
			dev->m_RTVDescriptorHeap
		);
	}

	// DSV Descriptors
	if (has_flag(info->bindFlags, SRBindFlag::DepthStencil)) {
		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {
			.Format = resourceDesc.Format,
			.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
			.Flags = D3D12_DSV_FLAG_NONE
		};

		D3D12_DEPTH_STENCIL_VIEW_DESC dsvReadOnlyDesc = {
			.Format = resourceDesc.Format,
			.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
			.Flags = D3D12_DSV_FLAG_READ_ONLY_DEPTH
		};

		internalTexture->dsvDescriptor = SRDX12Helpers::init_dsv_descriptor(
			dev->m_Device.Get(),
			internalTexture->allocation->GetResource(),
			dsvDesc,
			dev->m_DSVDescriptorHeap
		);
		internalTexture->dsvReadOnlyDescriptor = SRDX12Helpers::init_dsv_descriptor(
			dev->m_Device.Get(),
			internalTexture->allocation->GetResource(),
			dsvReadOnlyDesc,
			dev->m_DSVDescriptorHeap
		);
	}

	// SRV Descriptor
	if (has_flag(info->bindFlags, SRBindFlag::ShaderResource)) {
		DXGI_FORMAT srvFormat = resourceDesc.Format;

		if (info->format == SRFormat::D32_FLOAT) {
			srvFormat = DXGI_FORMAT_R32_FLOAT;
		}
		else if (info->format == SRFormat::D16_UNORM) {
			srvFormat = DXGI_FORMAT_R16_UNORM;
		}

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {
			.Format = srvFormat,
			.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
			.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
			.Texture2D = {
				.MostDetailedMip = 0,
				.MipLevels = info->mipLevels,
			}
		};

		internalTexture->srvDescriptor = SRDX12Helpers::init_srv_descriptor(
			dev->m_Device.Get(),
			internalTexture->allocation->GetResource(),
			srvDesc,
			dev->m_ResourceDescriptorHeap
		);
	}
}

void SRGFXDX12_CreateSampler(SRGFXDevice* device, const SRSamplerInfo* info, SRSampler* sampler) {
	auto* dev = (SRGFXDeviceDX12*)device->internalState;
	auto internalSampler = std::make_shared<SRSampler_DX12>();

	sampler->info = *info;
	sampler->type = SRResourceType::Sampler;
	sampler->internalState = internalSampler;

	D3D12_SAMPLER_DESC samplerDesc = {
		.Filter = to_dx12_filter(info->filter),
		.AddressU = to_dx12_texture_address_mode(info->addressU),
		.AddressV = to_dx12_texture_address_mode(info->addressV),
		.AddressW = to_dx12_texture_address_mode(info->addressW),
		.MipLODBias = info->mipLODBias,
		.MaxAnisotropy = info->maxAnisotropy,
		.ComparisonFunc = to_dx12_comparison_func(info->comparisonFunc),
		.MinLOD = info->minLOD,
		.MaxLOD = info->maxLOD
	};

	switch (info->borderColor) {
	case SRBorderColor::OpaqueBlack:
	{
		samplerDesc.BorderColor[0] = 0.0F;
		samplerDesc.BorderColor[1] = 0.0F;
		samplerDesc.BorderColor[2] = 0.0F;
		samplerDesc.BorderColor[3] = 1.0F;
	}
	break;
	case SRBorderColor::OpaqueWhite:
	{
		samplerDesc.BorderColor[0] = 1.0F;
		samplerDesc.BorderColor[1] = 1.0F;
		samplerDesc.BorderColor[2] = 1.0F;
		samplerDesc.BorderColor[3] = 1.0F;
	}
	break;
	default:
	{
		samplerDesc.BorderColor[0] = 0.0F;
		samplerDesc.BorderColor[1] = 0.0F;
		samplerDesc.BorderColor[2] = 0.0F;
		samplerDesc.BorderColor[3] = 0.0F;
	}
	break;
	}

	u32 index = dev->m_SamplerDescriptorHeap.get_next_index();

	internalSampler->samplerDescriptor = index;
	dev->m_Device->CreateSampler(
		&samplerDesc,
		dev->m_SamplerDescriptorHeap.get_cpu_handle(index)
	);
}

void SRGFXDX12_BindPipeline(SRGFXDevice* device, const SRPipeline* pipeline, const SRCmdList* cmdList) {
	auto* internalPipeline = to_dx12_internal(*pipeline);
	auto* internalCmdList = to_dx12_internal(*cmdList);

	internalCmdList->graphicsCmdList->SetPipelineState(internalPipeline->pipeline.Get());
	internalCmdList->graphicsCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	internalCmdList->graphicsCmdList->SetGraphicsRootSignature(internalPipeline->rootSignature.Get());
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
	auto* internalBuffer = to_dx12_internal(*buffer);
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
	auto* internalBuffer = to_dx12_internal(*buffer);
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
	auto* internalBuffer = to_dx12_internal(*buffer);
	auto* internalCmdList = to_dx12_internal(*cmdList);

	internalCmdList->graphicsCmdList->SetGraphicsRootConstantBufferView(
		1,
		internalBuffer->allocation->GetResource()->GetGPUVirtualAddress()
	);
}

void SRGFXDX12_PushConstants(SRGFXDevice* device, const void* data, u32 size, const SRCmdList* cmdList) {
	auto* internalCmdList = to_dx12_internal(*cmdList);
	assert(size <= 128);

	internalCmdList->graphicsCmdList->SetGraphicsRoot32BitConstants(
		0,
		size >> 2,
		data,
		0
	);
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
		auto* internalTexture = to_dx12_internal(*barrier->image.texture);

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

	if (dev->m_FrameCounter >= SR_GFX_FRAMES_IN_FLIGHT) {
		u64 needed = dev->m_FrameDoneValues[SRQueue_Universal][dev->m_FrameIndex];
		u64 current = dev->m_FrameFences[SRQueue_Universal]->GetCompletedValue();

		if (current < needed) {
			SR_DX12_CHECK(dev->m_FrameFences[SRQueue_Universal]->SetEventOnCompletion(needed, nullptr), "Wait for fence");
		}
	}

	auto* internalSwapchain = to_dx12_internal(*swapchain);
	dev->m_ImageIndex = internalSwapchain->swapchain->GetCurrentBackBufferIndex();
}

SRCmdList SRGFXDX12_BeginCommandList(SRGFXDevice* device, SRQueue queue) {
	auto* dev = (SRGFXDeviceDX12*)device->internalState;

	size_t& cmdListCounter = dev->m_PerFrameCmdListCounters[dev->m_FrameIndex];
	auto& cmdLists = dev->m_PerFrameCmdLists[dev->m_FrameIndex];

	if (cmdListCounter >= cmdLists.size()) {
		cmdLists.push_back(std::make_unique<SRCmdList_DX12>());
	}

	auto internalCmdList = cmdLists[cmdListCounter].get();
	if (internalCmdList->graphicsCmdList == nullptr) {
		// NOTE: We require ID3D12GraphicsCommandList7 to be available
		SR_DX12_CHECK(dev->m_Device->CreateCommandList(
			0,
			to_dx12_cmd_list_type(queue),
			dev->m_CommandAllocators[queue][dev->m_FrameIndex].Get(),
			nullptr,
			IID_PPV_ARGS(&internalCmdList->graphicsCmdList)
		), "Command list creation");

		// All command lists begin in recording state, so we close it
		internalCmdList->graphicsCmdList->Close();
	}

	SR_DX12_CHECK(dev->m_CommandAllocators[queue][dev->m_FrameIndex]->Reset(), "Reset command allocator");
	SR_DX12_CHECK(internalCmdList->graphicsCmdList->Reset(
		dev->m_CommandAllocators[queue][dev->m_FrameIndex].Get(),
		nullptr
	), "Begin command list recording");

	ID3D12DescriptorHeap* const descriptorHeaps[] = {
		dev->m_ResourceDescriptorHeap.get_heap_object(),
		dev->m_SamplerDescriptorHeap.get_heap_object()
	};
	internalCmdList->graphicsCmdList->SetDescriptorHeaps(std::size(descriptorHeaps), descriptorHeaps);

	++cmdListCounter;
	return SRCmdList{ internalCmdList };
}

void SRGFXDX12_BeginRenderPassSwapchain(SRGFXDevice* device, const SRSwapchain* swapchain, const SRCmdList* cmdList) {
	auto* dev = (SRGFXDeviceDX12*)device->internalState;
	auto* internalSwapchain = to_dx12_internal(*swapchain);
	auto* internalCmdList = to_dx12_internal(*cmdList);

	// NOTE: Stingray always assumes that the swapchain will never be cleared,
	// and thus we assume an immediate overwrite of the swapchain backbuffer.
	// Hence why D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD is used. The
	// reason for this is that swapchain clears are notoriously slow to perform,
	// and thus it was decided to not allow such clears.
	D3D12_CLEAR_VALUE clearValue = {
		.Format = to_dx12_format(swapchain->info.format),
		.Color = { 0.0F, 0.0F, 0.0F, 1.0F }
	};

	D3D12_RENDER_PASS_RENDER_TARGET_DESC passRTVDesc = {
		.cpuDescriptor = dev->m_RTVDescriptorHeap.get_cpu_handle(internalSwapchain->rtvDescriptors[dev->m_ImageIndex]),
		.BeginningAccess = {
			.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR,
			.Clear = { .ClearValue = clearValue }
		},
		.EndingAccess = {
			.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE
		}
	};

	// TODO: Transition layout
	SRImageTransitionInfo_DX12 transitionInfo = {
		.image = internalSwapchain->images[dev->m_ImageIndex].Get(),
		.oldLayout = D3D12_BARRIER_LAYOUT_PRESENT,
		.newLayout = D3D12_BARRIER_LAYOUT_RENDER_TARGET,
		.srcAccessMask = D3D12_BARRIER_ACCESS_NO_ACCESS,
		.dstAccessMask = D3D12_BARRIER_ACCESS_RENDER_TARGET,
		.srcStageMask = D3D12_BARRIER_SYNC_NONE,
		.dstStageMask = D3D12_BARRIER_SYNC_RENDER_TARGET
	};
	SRDX12Helpers::transition_image_layout(transitionInfo, internalCmdList->graphicsCmdList.Get());

	internalCmdList->graphicsCmdList->BeginRenderPass(
		1U,
		&passRTVDesc,
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
		auto* internalTexture = to_dx12_internal(*attachment->texture);
		assert(internalTexture);

		D3D12_RENDER_PASS_RENDER_TARGET_DESC rtvDesc = {
			.cpuDescriptor = dev->m_RTVDescriptorHeap.get_cpu_handle(internalTexture->rtvDescriptor),
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
		auto internalTexture = to_dx12_internal(*depthAttachment.texture);
		assert(internalTexture);

		if (depthAttachment.loadOp == SRLoadOp::Clear) {
			passDSVDesc.cpuDescriptor = dev->m_DSVDescriptorHeap.get_cpu_handle(internalTexture->dsvDescriptor);
			passDSVDesc.DepthBeginningAccess.Clear.ClearValue = {
				.Format = to_dx12_format(depthAttachment.texture->info.format),
				.DepthStencil = {
					.Depth = depthAttachment.clearValue,
					.Stencil = 0
				}
			};
		}
		else if (depthAttachment.loadOp == SRLoadOp::Load) {
			passDSVDesc.cpuDescriptor = dev->m_DSVDescriptorHeap.get_cpu_handle(internalTexture->dsvReadOnlyDescriptor);
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
	auto* internalSwapchain = to_dx12_internal(*swapchain);
	auto* internalCmdList = to_dx12_internal(*cmdList);
	internalCmdList->graphicsCmdList->EndRenderPass();

	SRImageTransitionInfo_DX12 transitionInfo = {
		.image = internalSwapchain->images[dev->m_ImageIndex].Get(),
		.oldLayout = D3D12_BARRIER_LAYOUT_RENDER_TARGET,
		.newLayout = D3D12_BARRIER_LAYOUT_PRESENT,
		.srcAccessMask = D3D12_BARRIER_ACCESS_RENDER_TARGET,
		.dstAccessMask = D3D12_BARRIER_ACCESS_NO_ACCESS,
		.srcStageMask = D3D12_BARRIER_SYNC_RENDER_TARGET,
		.dstStageMask = D3D12_BARRIER_SYNC_NONE,
	};
	SRDX12Helpers::transition_image_layout(transitionInfo, internalCmdList->graphicsCmdList.Get());
}

void SRGFXDX12_EndRenderPass(SRGFXDevice* device, const SRCmdList* cmdList) {
	auto* dev = (SRGFXDeviceDX12*)device->internalState;
	auto* internalCmdList = to_dx12_internal(*cmdList);
	internalCmdList->graphicsCmdList->EndRenderPass();
}

void SRGFXDX12_SubmitCommandLists(SRGFXDevice* device, const SRSwapchain* swapchain) {
	auto* dev = (SRGFXDeviceDX12*)device->internalState;
	auto* internalSwapchain = to_dx12_internal(*swapchain);

	u32 numSubmittedCmdLists = (u32)dev->m_PerFrameCmdListCounters[dev->m_FrameIndex];
	dev->m_PerFrameCmdListCounters[dev->m_FrameIndex] = 0ULL;

	std::vector<ID3D12CommandList*> cmdListsToSubmit;
	cmdListsToSubmit.reserve(numSubmittedCmdLists);
	for (u32 i = 0; i < numSubmittedCmdLists; ++i) {
		SRCmdList_DX12* cmdList = dev->m_PerFrameCmdLists[dev->m_FrameIndex][i].get();
		SR_DX12_CHECK(cmdList->graphicsCmdList->Close(), "Close command list");
		cmdListsToSubmit.push_back(cmdList->graphicsCmdList.Get());
	}

	dev->m_CommandQueues[SRQueue_Universal]->ExecuteCommandLists(
		numSubmittedCmdLists,
		cmdListsToSubmit.data()
	);

	SR_DX12_CHECK(dev->m_CommandQueues[SRQueue_Universal]->Signal(
		dev->m_FrameFences[SRQueue_Universal],
		dev->m_NextGPUSignalValue
	), "Signal fence");

	UINT syncInterval = swapchain->info.vSync ? 1 : 0;
	UINT presentFlags = dev->m_IsTearingSupported && !swapchain->info.vSync ? DXGI_PRESENT_ALLOW_TEARING : 0;
	internalSwapchain->swapchain->Present(syncInterval, presentFlags);

	dev->m_FrameDoneValues[SRQueue_Universal][dev->m_FrameIndex] = dev->m_NextGPUSignalValue++;
	dev->m_FrameIndex = (dev->m_FrameIndex + 1) % SR_GFX_FRAMES_IN_FLIGHT;
	++dev->m_FrameCounter;
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

void SRGFXDX12_DispatchMesh(SRGFXDevice* device, u32 x, u32 y, u32 z, const SRCmdList* cmdList) {
	auto* internalCmdList = to_dx12_internal(*cmdList);
	internalCmdList->graphicsCmdList->DispatchMesh(x, y, z);
}

SRDescriptorIndex SRGFXDX12_GetDescriptorIndexSRV(SRGFXDevice* device, const SRResource* resource) {
	if (resource->type == SRResourceType::Texture) {
		auto* internalTexture = (SRTexture_DX12*)resource->internalState.get();
		return internalTexture->srvDescriptor;
	}
	if (resource->type == SRResourceType::Buffer) {
		auto* internalBuffer = (SRBuffer_DX12*)resource->internalState.get();
		return internalBuffer->srvDescriptor;
	}

	assert(false);
	return INVALID_DESCRIPTOR_INDEX;
}

SRShaderPlatformInfo SRGFXDX12_GetShaderPlatformInfo(SRGFXDevice* device) {
	auto* dev = (SRGFXDeviceDX12*)device->internalState;
	return dev->m_ShaderPlatformInfo;
}

void SRGFXDX12_WaitForGPU(SRGFXDevice* device) {
	auto* dev = (SRGFXDeviceDX12*)device->internalState;

	// TODO: Right now we only use universal queue, remember that when we add
	// dedicated compute/copy queue we also need to Signal those here.
	u64 target = ++dev->m_NextGPUSignalValue;
	SR_DX12_CHECK(dev->m_CommandQueues[SRQueue_Universal]->Signal(
		dev->m_FrameFences[SRQueue_Universal],
		target
	), "Signal fence");

	if (dev->m_FrameFences[SRQueue_Universal]->GetCompletedValue() < target) {
		SR_DX12_CHECK(dev->m_FrameFences[SRQueue_Universal]->SetEventOnCompletion(
			target,
			nullptr
		), "Wait for fence");
	}
}

void SRGFXDX12_FlushInitialUploads(SRGFXDevice* device) {
	auto* dev = (SRGFXDeviceDX12*)device->internalState;

	SR_DX12_CHECK(dev->m_UploadCmdList->Close(), "Close command list");

	ID3D12CommandList* cmdLists[1] = { dev->m_UploadCmdList.Get() };
	dev->m_CommandQueues[SRQueue_Copy]->ExecuteCommandLists(
		1,
		cmdLists
	);

	// TEMPORARY
	ComPtr<ID3D12Fence> tempFence;
	SR_DX12_CHECK(dev->m_Device->CreateFence(
		0,
		D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(&tempFence)
	), "Create temporary fence");

	SR_DX12_CHECK(dev->m_CommandQueues[SRQueue_Copy]->Signal(tempFence.Get(), 1), "Signal fence");

	if (tempFence->GetCompletedValue() < 1) {
		SR_DX12_CHECK(tempFence->SetEventOnCompletion(1, nullptr), "Wait for fence");
	}

	dev->m_PendingUploadResources.clear();
	dev->m_UploadCmdAllocator->Reset();
}

void SRGFXDX12_SetupImGuiInitInfo(SRGFXDevice* device, SRFormat swapchainFormat) {
	auto* dev = (SRGFXDeviceDX12*)device->internalState;

	ImGui_ImplDX12_InitInfo initInfo = {};
	initInfo.Device = dev->m_Device.Get();
	initInfo.CommandQueue = dev->m_CommandQueues[SRQueue_Universal];
	initInfo.NumFramesInFlight = SR_GFX_FRAMES_IN_FLIGHT;
	initInfo.RTVFormat = to_dx12_format(swapchainFormat);
	initInfo.UserData = &dev->m_ResourceDescriptorHeap;
	initInfo.SrvDescriptorHeap = dev->m_ResourceDescriptorHeap.get_heap_object();

	initInfo.SrvDescriptorAllocFn = [](
		ImGui_ImplDX12_InitInfo* initInfo,
		D3D12_CPU_DESCRIPTOR_HANDLE* cpuHandle,
		D3D12_GPU_DESCRIPTOR_HANDLE* gpuHandle
	) {
		SRDescriptorHeap_DX12* descriptorHeap = reinterpret_cast<SRDescriptorHeap_DX12*>(
			initInfo->UserData
		);

		SRDescriptorIndex descriptorIndex = descriptorHeap->get_next_index();
		*cpuHandle = descriptorHeap->get_cpu_handle(descriptorIndex);
		*gpuHandle = descriptorHeap->get_gpu_handle(descriptorIndex);
	};

	initInfo.SrvDescriptorFreeFn = [](
		ImGui_ImplDX12_InitInfo* initInfo,
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
		D3D12_GPU_DESCRIPTOR_HANDLE
	) {
		SRDescriptorHeap_DX12* descriptorHeap = reinterpret_cast<SRDescriptorHeap_DX12*>(
			initInfo->UserData
		);

		// NOTE: CPU and GPU handle are related, freeing CPU also frees GPU
		SRDescriptorIndex descriptorIndex = descriptorHeap->get_index_from_handle(cpuHandle);
		descriptorHeap->free_index(descriptorIndex);
	};

	ImGui_ImplDX12_Init(&initInfo);
}
