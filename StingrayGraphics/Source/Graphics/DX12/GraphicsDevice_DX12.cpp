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

using namespace Microsoft::WRL;
extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = 618; }
extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\"; }

// ------------------------------ Impl Definition ------------------------------
struct SRGraphicsDevice_DX12::Impl {
	Impl(SRWindow& window) : m_Window(window) {}
	~Impl();

	void create_debug_interface();
	void create_dxgi_debug_interface();
	void create_dxgi_factory();
	void create_device();
	void create_memory_allocator();
	void create_command_allocators();
	void create_command_queues();
	void create_sync_objects();
	void create_descriptor_heaps();

	void create_swapchain(const SRSwapchainInfo& info, SRSwapchain& swapchain);
	void create_pipeline(const SRPipelineInfo& info, SRPipeline& pipeline);
	void create_buffer(const SRBufferInfo& info, SRBuffer& buffer, const void* data);
	void create_texture(const SRTextureInfo& info, SRTexture& texture, const SRSubresourceData* data);
	void create_sampler(const SRSamplerInfo& info, SRSampler& sampler);

	void bind_pipeline(const SRPipeline& pipeline, const SRCmdList& cmdList);
	void bind_viewport(const SRViewport& viewport, const SRCmdList& cmdList);
	void bind_vertex_buffer(const SRBuffer& buffer, const SRCmdList& cmdList);
	void bind_index_buffer(const SRBuffer& buffer, const SRCmdList& cmdList);
	void bind_root_constant_buffer(const SRBuffer& buffer, const SRCmdList& cmdList);
	void push_constants(const void* data, u32 size, const SRCmdList& cmdList);
	void barrier(const SRBarrier* pBarriers, u32 numBarriers, const SRCmdList& cmdList);

	SRCmdList begin_command_list(SRQueue queue);
	void begin_render_pass(const SRSwapchain& swapchain, const SRCmdList& cmdList);
	void begin_render_pass(const SRPassInfo& passInfo, const SRCmdList& cmdList);
	void end_render_pass(const SRSwapchain& swapchain, const SRCmdList& cmdList);
	void end_render_pass(const SRCmdList& cmdList);
	void submit_command_lists(const SRSwapchain& swapchain);

	void draw(u32 vtxCount, u32 startVtx, const SRCmdList& cmdList);
	void draw_indexed(u32 idxCount, u32 startIdx, u32 baseVtx, const SRCmdList& cmdList);
	void dispatch_mesh(u32 groupCountX, u32 groupCountY, u32 groupCountZ, const SRCmdList& cmdList);

	SRShaderPlatformInfo get_shader_platform_info();
	SRDescriptorIndex get_descriptor_index_srv(const SRResource& resource);
	void wait_for_gpu();

	// NOTE: TEMPORARY STUFF
	void flush_initial_uploads();
	std::vector<std::shared_ptr<void>> m_PendingUploadResources;
	// END OF TEMPORARY STUFF

	void setup_imgui_init_info(SRFormat swapchainFormat);

	SRWindow& m_Window;
	#ifdef _DEBUG
	ComPtr<ID3D12Debug> m_DebugInterface;
	ComPtr<IDXGIInfoQueue> m_DXGIDebugInfoQueue;
	ComPtr<ID3D12DebugDevice> m_DebugDevice;
	#endif

	ComPtr<ID3D12CommandAllocator> m_UploadCmdAllocator;
	ComPtr<ID3D12GraphicsCommandList7> m_UploadCmdList;
	bool m_IsUploadCmdListRecording = false;

	ComPtr<IDXGIFactory2> m_DXGIFactory;
	ComPtr<IDXGIAdapter1> m_Adapter;
	D3D12MA::Allocator* m_Allocator = nullptr;
	ComPtr<ID3D12Device10> m_Device;
	SRDeviceCapabilities_DX12 m_DeviceCapabilities = {};
	ComPtr<ID3D12CommandAllocator> m_CommandAllocators[SRQueue_COUNT][FRAMES_IN_FLIGHT];
	ComPtr<ID3D12CommandQueue> m_CommandQueues[SRQueue_COUNT];

	SRDescriptorHeap_DX12 m_ResourceDescriptorHeap = { D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, MAX_RESOURCE_DESCRIPTORS }; // CBV + SRV + UAV
	SRDescriptorHeap_DX12 m_SamplerDescriptorHeap = { D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, MAX_SAMPLER_DESCRIPTORS };
	SRDescriptorHeap_DX12 m_RTVDescriptorHeap = { D3D12_DESCRIPTOR_HEAP_TYPE_RTV, MAX_RTV_DESCRIPTORS };
	SRDescriptorHeap_DX12 m_DSVDescriptorHeap = { D3D12_DESCRIPTOR_HEAP_TYPE_DSV, MAX_DSV_DESCRIPTORS };

	ComPtr<ID3D12Fence> m_FrameFences[SRQueue_COUNT];
	u64 m_FrameDoneValues[SRQueue_COUNT][FRAMES_IN_FLIGHT] = {};
	u64 m_NextGPUSignalValue = 1;

	std::vector<std::unique_ptr<SRCmdList_DX12>> m_PerFrameCmdLists[FRAMES_IN_FLIGHT];
	size_t m_PerFrameCmdListCounters[FRAMES_IN_FLIGHT] = {};
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

// -------------------------- Impl Method Definitions --------------------------
SRGraphicsDevice_DX12::Impl::~Impl() {
	m_Allocator->Release();
	m_Allocator = nullptr;
}

void SRGraphicsDevice_DX12::Impl::create_debug_interface() {
#ifdef _DEBUG
	if (FAILED(D3D12GetDebugInterface(IID_PPV_ARGS(&m_DebugInterface)))) {
		SRLOG_WARN_CAT(SRLOG_CAT_DX12, "Failed to create base ID3D12Debug interface. Debug information will be limited");
		return;
	}

	m_DebugInterface->EnableDebugLayer();

	ComPtr<ID3D12Debug1> debugInterface1;
	if (FAILED(m_DebugInterface.As(&debugInterface1))) {
		SRLOG_WARN_CAT(SRLOG_CAT_DX12, "Failed to create ID3D12Debug1 interface. GBV/SCQV information will not be available");
		return;
	}

	debugInterface1->SetEnableGPUBasedValidation(TRUE);
	debugInterface1->SetEnableSynchronizedCommandQueueValidation(TRUE);
#else
	return;
#endif
}

void SRGraphicsDevice_DX12::Impl::create_dxgi_debug_interface() {
#ifdef _DEBUG
	if (FAILED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&m_DXGIDebugInfoQueue)))) {
		SRLOG_WARN_CAT(SRLOG_CAT_DX12, "Failed to create DXGI debug interface. DXGI-related information will not be available");
		return;
	}

	SR_DX12_CHECK(
		m_DXGIDebugInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, TRUE),
		"Set break on severity"
	);
	SR_DX12_CHECK(
		m_DXGIDebugInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, TRUE),
		"Set break on severity"
	);
#else
	return;
#endif
}

void SRGraphicsDevice_DX12::Impl::create_dxgi_factory() {
	UINT dxgiFactoryFlags = 0;
	#ifdef _DEBUG
		dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
	#endif

	SR_DX12_CHECK(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&m_DXGIFactory)), "DXGI factory creation");
	SRLOG_DEBUG_CAT(SRLOG_CAT_DX12, "Successfully created DXGI factory");

	ComPtr<IDXGIFactory5> dxgiFactory5;
	if (SUCCEEDED(m_DXGIFactory.As(&dxgiFactory5))) {
		BOOL allowTearing = FALSE;
		const HRESULT hr = dxgiFactory5->CheckFeatureSupport(
			DXGI_FEATURE_PRESENT_ALLOW_TEARING,
			&allowTearing,
			sizeof(allowTearing)
		);

		m_IsTearingSupported = SUCCEEDED(hr) && allowTearing == TRUE;
	}


}

void SRGraphicsDevice_DX12::Impl::create_device() {
	u32 pickedDeviceIdx = ~0U;
	std::string deviceName;

	// NOTE: We prefer IDXGIFactory6 since it allows us to enumerate adapters
	// based on GPU preference. If it's not available, we pick a device with
	// EnumAdapters1 instead.
	ComPtr<IDXGIFactory6> dxgiFactory6;
	bool isDXGIFactory6Available = SUCCEEDED(m_DXGIFactory.As(&dxgiFactory6));
	
	for (UINT i = 0;; ++i) {
		ComPtr<IDXGIAdapter1> adapter;
		HRESULT hr;

		if (isDXGIFactory6Available) {
			hr = dxgiFactory6->EnumAdapterByGpuPreference(
				i,
				DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
				IID_PPV_ARGS(&adapter)
			);
		}
		else {
			hr = m_DXGIFactory->EnumAdapters1(i, adapter.GetAddressOf());
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

		SR_DX12_CHECK(device.As(&m_Device), "Create as ID3D12Device10");
		m_Adapter = adapter;

		#ifdef _DEBUG
			if (FAILED(m_Device.As(&m_DebugDevice))) {
				SRLOG_DEBUG_CAT(SRLOG_CAT_DX12, "ID3D12DebugDevice not available. D3D12 object reporting disabled");
			}
		#endif

		m_DeviceCapabilities = capabilities;
		pickedDeviceIdx = i;
		break;
	}

	if (pickedDeviceIdx == ~0) {
		SRLOG_CRITICAL_CAT(SRLOG_CAT_DX12, "No suitable GPU found");
		throw std::runtime_error("DX12 ERROR: No suitable GPU found");
	}

	SRLOG_INFO_CAT(SRLOG_CAT_DX12, "Picked [GPU%u] %s", pickedDeviceIdx, deviceName.c_str());
}

void SRGraphicsDevice_DX12::Impl::create_memory_allocator() {
	const D3D12MA::ALLOCATOR_DESC allocatorDesc = {
		.Flags = D3D12MA_RECOMMENDED_ALLOCATOR_FLAGS,
		.pDevice = m_Device.Get(),
		.pAdapter = m_Adapter.Get()
	};

	SR_DX12_CHECK(D3D12MA::CreateAllocator(&allocatorDesc, &m_Allocator), "Create D3D12 Memory Allocator");
}

void SRGraphicsDevice_DX12::Impl::create_command_allocators() {
	for (u32 f = 0; f < FRAMES_IN_FLIGHT; ++f) {
		// Universal command allocators (direct)
		SR_DX12_CHECK(m_Device->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(&m_CommandAllocators[SRQueue_Universal][f])
		), "Command allocator creation");

		// TODO: Create command allocators for other queue types too. We will need
		// it eventually.

		// Compute (TODO)

		// Copy command allocators (TODO)
	}

	// TEMPORARY
	SR_DX12_CHECK(m_Device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_COPY,
		IID_PPV_ARGS(&m_UploadCmdAllocator)
	), "Upload command allocator creation");

	SR_DX12_CHECK(m_Device->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_COPY,
		m_UploadCmdAllocator.Get(),
		nullptr,
		IID_PPV_ARGS(&m_UploadCmdList)
	), "Command list creation");
	m_UploadCmdList->Close();
}

void SRGraphicsDevice_DX12::Impl::create_command_queues() {
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc = {
		.Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
		.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
		.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
		.NodeMask = 0
	};

	SR_DX12_CHECK(m_Device->CreateCommandQueue(
		&commandQueueDesc,
		IID_PPV_ARGS(&m_CommandQueues[SRQueue_Universal])
	), "Command queue creation");

	commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;

	SR_DX12_CHECK(m_Device->CreateCommandQueue(
		&commandQueueDesc,
		IID_PPV_ARGS(&m_CommandQueues[SRQueue_Copy])
	), "Command queue creation");

	// TODO: Create command queues for other queue types too. We will need it
	// eventually.
}


void SRGraphicsDevice_DX12::Impl::create_sync_objects() {
	for (u32 q = 0; q < SRQueue_COUNT; ++q) {
		SR_DX12_CHECK(m_Device->CreateFence(
			0,
			D3D12_FENCE_FLAG_NONE,
			IID_PPV_ARGS(&m_FrameFences[q])
		), "Create frame fence");
	}
}

void SRGraphicsDevice_DX12::Impl::create_descriptor_heaps() {
	m_ResourceDescriptorHeap.initialize(m_Device.Get());
	m_SamplerDescriptorHeap.initialize(m_Device.Get());
	m_RTVDescriptorHeap.initialize(m_Device.Get());
	m_DSVDescriptorHeap.initialize(m_Device.Get());
}

void SRGraphicsDevice_DX12::Impl::create_swapchain(const SRSwapchainInfo& info, SRSwapchain& swapchain) {
	auto internalSwapchain = std::make_shared<SRSwapchain_DX12>();

	swapchain.info = info;
	swapchain.internalState = internalSwapchain;

	// TODO: Allow for swapchain recreation
	const DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {
		.Width = info.width,
		.Height = info.height,
		.Format = to_dx12_format(info.format),
		.Stereo = FALSE,
		.SampleDesc = { .Count = 1U, .Quality = 0U },
		.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
		.BufferCount = info.numBuffers,
		.Scaling = DXGI_SCALING_NONE,
		.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
		.AlphaMode = DXGI_ALPHA_MODE_IGNORE,
		.Flags = m_IsTearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0U
	};

	const HWND windowHandle = (HWND)m_Window.get_internal_handle();
	ComPtr<IDXGISwapChain1> dxgiSwapchain1;
	SR_DX12_CHECK(m_DXGIFactory->CreateSwapChainForHwnd(
		m_CommandQueues[SRQueue_Universal].Get(),
		windowHandle,
		&swapchainDesc,
		nullptr,
		nullptr,
		dxgiSwapchain1.GetAddressOf()
	), "Swapchain creation");
	SR_DX12_CHECK(dxgiSwapchain1.As(&internalSwapchain->swapchain), "Convert IDXGISwapchain1 to IDXGISwapchain3");
	SR_DX12_CHECK(m_DXGIFactory->MakeWindowAssociation(windowHandle, DXGI_MWA_NO_ALT_ENTER), "Disable Alt+Enter");

	internalSwapchain->images.resize(info.numBuffers);
	internalSwapchain->rtvDescriptors.reserve(info.numBuffers);

	for (u32 i = 0; i < info.numBuffers; ++i) {
		SR_DX12_CHECK(internalSwapchain->swapchain->GetBuffer(
			i,
			IID_PPV_ARGS(&internalSwapchain->images[i])
		), "Get backbuffer");

		const SRDescriptorIndex rtvIndex = m_RTVDescriptorHeap.get_next_index();
		m_Device->CreateRenderTargetView(
			internalSwapchain->images[i].Get(),
			nullptr,
			m_RTVDescriptorHeap.get_cpu_handle(rtvIndex)
		);
		internalSwapchain->rtvDescriptors.push_back(rtvIndex);
	}
}

void SRGraphicsDevice_DX12::Impl::create_pipeline(const SRPipelineInfo& info, SRPipeline& pipeline) {
	auto internalPipeline = std::make_shared<SRPipeline_DX12>();
	pipeline.info = info;
	pipeline.internalState = internalPipeline;

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

	const D3D12_ROOT_PARAMETER1 rootConstant = {
		.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
		.Constants = { .ShaderRegister = 0, .RegisterSpace = 0, .Num32BitValues = 32 /* 128 bytes */ },
		.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
	};
	const D3D12_ROOT_PARAMETER1 perFrameCBV = {
		.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV,
		.Descriptor = {
			.ShaderRegister = 1,
			.RegisterSpace = 0,
			.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE
		},
		.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
	};
	const D3D12_ROOT_PARAMETER1 rootParameters[] = {
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

	if (!info.meshShader) {
		rootSignatureDesc.Desc_1_1.Flags |= D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	}

	ComPtr<ID3DBlob> rootSignatureBlob = nullptr;
	ComPtr<ID3DBlob> rootSignatureErrorBlob = nullptr;
	SR_DX12_CHECK(D3D12SerializeVersionedRootSignature(
		&rootSignatureDesc,
		&rootSignatureBlob,
		&rootSignatureErrorBlob
	), "Serialize versioned root signature");

	SR_DX12_CHECK(m_Device->CreateRootSignature(
		0U,
		rootSignatureBlob->GetBufferPointer(),
		rootSignatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&internalPipeline->rootSignature)
	), "Create root signature");
	psoStream.rootSignature = internalPipeline->rootSignature.Get();

	if (info.vertexShader != nullptr) {
		psoStream.vertexShader = { info.vertexShader->byteCode.data(), info.vertexShader->byteCode.size(), };
	}
	if (info.pixelShader != nullptr) {
		psoStream.pixelShader = { info.pixelShader->byteCode.data(), info.pixelShader->byteCode.size() };
	}
	if (info.meshShader != nullptr) {
		psoStream.meshShader = { info.meshShader->byteCode.data(), info.meshShader->byteCode.size() };
	}
	if (info.taskShader != nullptr) {
		psoStream.taskShader = { info.taskShader->byteCode.data(), info.taskShader->byteCode.size() };
	}

	// Rasterizer state
	CD3DX12_RASTERIZER_DESC rasterizerDesc = {};
	rasterizerDesc.FillMode = to_dx12_fill_mode(info.rasterizerState.fillMode);
	rasterizerDesc.CullMode = to_dx12_cull_mode(info.rasterizerState.cullMode);
	rasterizerDesc.FrontCounterClockwise = info.rasterizerState.frontCW ? TRUE : FALSE;
	rasterizerDesc.DepthBias = info.rasterizerState.depthBias;
	rasterizerDesc.DepthBiasClamp = info.rasterizerState.depthBiasClamp;
	rasterizerDesc.SlopeScaledDepthBias = info.rasterizerState.slopeScaledDepthBias;
	rasterizerDesc.DepthClipEnable = info.rasterizerState.depthClipEnable ? TRUE : FALSE;
	rasterizerDesc.MultisampleEnable = info.rasterizerState.multisampleEnable ? TRUE : FALSE;
	rasterizerDesc.AntialiasedLineEnable = info.rasterizerState.antialisedLineEnable ? TRUE : FALSE;
	rasterizerDesc.ForcedSampleCount = 0U;
	rasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
	psoStream.rasterizerState = rasterizerDesc;

	// Depth stencil state
	CD3DX12_DEPTH_STENCIL_DESC1 depthStencilDesc = {};
	depthStencilDesc.DepthEnable = info.depthStencilState.depthEnable ? TRUE : FALSE;
	depthStencilDesc.DepthWriteMask = to_dx12_depth_write_mask(info.depthStencilState.depthWriteMask);
	depthStencilDesc.DepthFunc = to_dx12_comparison_func(info.depthStencilState.depthFunction);
	depthStencilDesc.StencilEnable = info.depthStencilState.stencilEnable ? TRUE : FALSE;
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
	blendDesc.AlphaToCoverageEnable = info.blendState.alphaToCoverage ? TRUE : FALSE;
	blendDesc.IndependentBlendEnable = info.blendState.independentBlend ? TRUE : FALSE;
	for (size_t i = 0; i < 8; ++i) {
		const auto& blendState = info.blendState.renderTargetBlendStates[i];
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
	inputLayoutDesc.NumElements = static_cast<UINT>(info.inputLayout.elements.size());
	std::vector<D3D12_INPUT_ELEMENT_DESC> inputElements;
	inputElements.reserve(info.inputLayout.elements.size());

	for (size_t i = 0; i < info.inputLayout.elements.size(); ++i) {
		const auto& element = info.inputLayout.elements[i];
		const D3D12_INPUT_ELEMENT_DESC dx12Element = {
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
	psoStream.depthStencilFormat = to_dx12_format(info.depthStencilFormat);

	// RTV formats
	D3D12_RT_FORMAT_ARRAY rtvFormats = {};
	rtvFormats.NumRenderTargets = info.numRenderTargets;
	for (size_t i = 0; i < info.numRenderTargets; ++i) {
		rtvFormats.RTFormats[i] = to_dx12_format(info.renderTargetFormats[i]);
	}
	psoStream.formats = rtvFormats;

	// Sample desc
	DXGI_SAMPLE_DESC sampleDesc = {};
	sampleDesc.Count = 1U;
	sampleDesc.Quality = 0U;
	psoStream.sampleDesc = sampleDesc;

	// Sample mask
	psoStream.sampleMask = UINT_MAX;

	const D3D12_PIPELINE_STATE_STREAM_DESC psoStreamDesc = {
		.SizeInBytes = sizeof(PSOStream),
		.pPipelineStateSubobjectStream = &psoStream
	};

	SR_DX12_CHECK(m_Device->CreatePipelineState(
		&psoStreamDesc,
		IID_PPV_ARGS(&internalPipeline->pipeline)
	), "Create pipeline state");
}

void SRGraphicsDevice_DX12::Impl::create_buffer(const SRBufferInfo& info, SRBuffer& buffer, const void* data) {
	auto internalBuffer = std::make_shared<SRBuffer_DX12>();

	buffer.type = SRResourceType::Buffer;
	buffer.info = info;
	buffer.internalState = internalBuffer;
	buffer.mappedData = nullptr;
	buffer.mappedSize = 0;

	D3D12_RESOURCE_DESC1 resourceDesc = {
		.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
		.Alignment = 0,
		.Width = info.size,
		.Height = 1,
		.DepthOrArraySize = 1,
		.MipLevels = 1,
		.Format = DXGI_FORMAT_UNKNOWN,
		.SampleDesc = { .Count = 1, .Quality = 0 },
		.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
		.Flags = D3D12_RESOURCE_FLAG_NONE
	};

	if (has_flag(info.bindFlags, SRBindFlag::UnorderedAccess)) {
		resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	}
	// TODO: Look into whether or not this is always correct
	if (!has_flag(info.bindFlags, SRBindFlag::ShaderResource)) {
		resourceDesc.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
	}

	D3D12MA::ALLOCATION_DESC allocDesc = {
		.HeapType = D3D12_HEAP_TYPE_DEFAULT,
	};

	switch (info.usage) {
	case SRUsage::Default:
		allocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
		break;
	case SRUsage::Upload:
		allocDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
		break;
	}
	SR_DX12_CHECK(m_Allocator->CreateResource3(
		&allocDesc,
		&resourceDesc,
		D3D12_BARRIER_LAYOUT_UNDEFINED,
		nullptr,
		0,
		nullptr,
		&internalBuffer->allocation,
		IID_NULL, nullptr
	), "CreateResource3");

	if (info.usage == SRUsage::Default && data != nullptr) {
		// Staging buffer
		SRBufferInfo stagingBufferInfo = info;
		stagingBufferInfo.usage = SRUsage::Upload;
		stagingBufferInfo.bindFlags = SRBindFlag::None;
		stagingBufferInfo.miscFlags = SRMiscFlag::None;

		SRBuffer stagingBuffer;
		create_buffer(stagingBufferInfo, stagingBuffer, data);

		m_PendingUploadResources.push_back(stagingBuffer.internalState);
		auto internalStagingBuffer = to_dx12_internal(stagingBuffer);

		// Copy staging buffer into target buffer
		if (!m_IsUploadCmdListRecording) {
			SR_DX12_CHECK(m_UploadCmdAllocator->Reset(), "Reset command allocator");
			SR_DX12_CHECK(m_UploadCmdList->Reset(
				m_UploadCmdAllocator.Get(),
				nullptr
			), "Begin upload command list recording");

			m_IsUploadCmdListRecording = true;
		}

		ID3D12Resource* srcResource = internalStagingBuffer->allocation->GetResource();
		ID3D12Resource* dstResource = internalBuffer->allocation->GetResource();
		m_UploadCmdList->CopyResource(dstResource, srcResource);
	}
	else if (info.usage == SRUsage::Upload) {
		internalBuffer->allocation->GetResource()->Map(0, nullptr, &buffer.mappedData);
		buffer.mappedSize = info.size;

		if (data != nullptr) {
			std::memcpy(buffer.mappedData, data, info.size);
		}

		// NOTE: We always perform persistent mappings, so no unmap is necessary
	}

	// Descriptors
	// TODO: UAV
	if (has_flag(info.bindFlags, SRBindFlag::ShaderResource)) {
		if (has_flag(info.miscFlags, SRMiscFlag::StructuredBuffer)) {
			const D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {
				.Format = DXGI_FORMAT_UNKNOWN,
				.ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
				.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
				.Buffer = {
					.FirstElement = 0,
					.NumElements = static_cast<UINT>(info.size / info.stride),
					.StructureByteStride = info.stride,
					.Flags = D3D12_BUFFER_SRV_FLAG_NONE
				}
			};

			internalBuffer->srvDescriptor = SRDX12Helpers::init_srv_descriptor(
				m_Device.Get(),
				internalBuffer->allocation->GetResource(),
				srvDesc,
				m_ResourceDescriptorHeap
			);
		}
	}
	// TODO: CBV
}

void SRGraphicsDevice_DX12::Impl::create_texture(const SRTextureInfo& info, SRTexture& texture, const SRSubresourceData* data) {
	assert(info.usage == SRUsage::Default);
	auto internalTexture = std::make_shared<SRTexture_DX12>();

	texture.info = info;
	texture.internalState = internalTexture;
	texture.type = SRResourceType::Texture;

	D3D12_RESOURCE_DESC1 resourceDesc = {
		.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
		.Alignment = 0,
		.Width = static_cast<UINT64>(info.width),
		.Height = info.height,
		.DepthOrArraySize = static_cast<UINT16>(info.depth),
		.MipLevels = static_cast<UINT16>(info.mipLevels),
		.Format = to_dx12_format(info.format),
		.SampleDesc = {.Count = info.sampleCount, .Quality = 0 },
		.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
		.Flags = D3D12_RESOURCE_FLAG_NONE
	};

	// Bind flags
	if (has_flag(info.bindFlags, SRBindFlag::DepthStencil)) {
		resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	}
	if (has_flag(info.bindFlags, SRBindFlag::UnorderedAccess)) {
		resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	}
	if (has_flag(info.bindFlags, SRBindFlag::RenderTarget)) {
		resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	}

	D3D12MA::ALLOCATION_DESC allocDesc = {
		.HeapType = D3D12_HEAP_TYPE_DEFAULT,
	};
	SR_DX12_CHECK(m_Allocator->CreateResource3(
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
	if (has_flag(info.bindFlags, SRBindFlag::RenderTarget)) {
		const D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {
			.Format = resourceDesc.Format,
			.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D
		};

		internalTexture->rtvDescriptor = SRDX12Helpers::init_rtv_descriptor(
			m_Device.Get(),
			internalTexture->allocation->GetResource(),
			rtvDesc,
			m_RTVDescriptorHeap
		);
	}

	// DSV Descriptors
	if (has_flag(info.bindFlags, SRBindFlag::DepthStencil)) {
		const D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {
			.Format = resourceDesc.Format,
			.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
			.Flags = D3D12_DSV_FLAG_NONE
		};

		const D3D12_DEPTH_STENCIL_VIEW_DESC dsvReadOnlyDesc = {
			.Format = resourceDesc.Format,
			.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
			.Flags = D3D12_DSV_FLAG_READ_ONLY_DEPTH
		};

		internalTexture->dsvDescriptor = SRDX12Helpers::init_dsv_descriptor(
			m_Device.Get(),
			internalTexture->allocation->GetResource(),
			dsvDesc,
			m_DSVDescriptorHeap
		);
		internalTexture->dsvReadOnlyDescriptor = SRDX12Helpers::init_dsv_descriptor(
			m_Device.Get(),
			internalTexture->allocation->GetResource(),
			dsvReadOnlyDesc,
			m_DSVDescriptorHeap
		);
	}

	// SRV Descriptor
	if (has_flag(info.bindFlags, SRBindFlag::ShaderResource)) {
		DXGI_FORMAT srvFormat = resourceDesc.Format;

		if (info.format == SRFormat::D32_FLOAT) {
			srvFormat = DXGI_FORMAT_R32_FLOAT;
		}
		else if (info.format == SRFormat::D16_UNORM) {
			srvFormat = DXGI_FORMAT_R16_UNORM;
		}

		const D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {
			.Format = srvFormat,
			.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
			.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
			.Texture2D = {
				.MostDetailedMip = 0,
				.MipLevels = info.mipLevels,
			}
		};

		internalTexture->srvDescriptor = SRDX12Helpers::init_srv_descriptor(
			m_Device.Get(),
			internalTexture->allocation->GetResource(),
			srvDesc,
			m_ResourceDescriptorHeap
		);
	}
}

void SRGraphicsDevice_DX12::Impl::create_sampler(const SRSamplerInfo& info, SRSampler& sampler) {
	auto internalSampler = std::make_shared<SRSampler_DX12>();

	sampler.info = info;
	sampler.type = SRResourceType::Sampler;
	sampler.internalState = internalSampler;

	D3D12_SAMPLER_DESC samplerDesc = {
		.Filter = to_dx12_filter(info.filter),
		.AddressU = to_dx12_texture_address_mode(info.addressU),
		.AddressV = to_dx12_texture_address_mode(info.addressV),
		.AddressW = to_dx12_texture_address_mode(info.addressW),
		.MipLODBias = info.mipLODBias,
		.MaxAnisotropy = info.maxAnisotropy,
		.ComparisonFunc = to_dx12_comparison_func(info.comparisonFunc),
		.MinLOD = info.minLOD,
		.MaxLOD = info.maxLOD
	};

	switch (info.borderColor) {
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

	const u32 index = m_SamplerDescriptorHeap.get_next_index();

	internalSampler->samplerDescriptor = index;
	m_Device->CreateSampler(
		&samplerDesc,
		m_SamplerDescriptorHeap.get_cpu_handle(index)
	);
}

void SRGraphicsDevice_DX12::Impl::bind_pipeline(const SRPipeline& pipeline, const SRCmdList& cmdList) {
	auto* internalPipeline = to_dx12_internal(pipeline);
	auto* internalCmdList = to_dx12_internal(cmdList);

	internalCmdList->graphicsCmdList->SetPipelineState(internalPipeline->pipeline.Get());
	internalCmdList->graphicsCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	internalCmdList->graphicsCmdList->SetGraphicsRootSignature(internalPipeline->rootSignature.Get());
}

void SRGraphicsDevice_DX12::Impl::bind_viewport(const SRViewport& viewport, const SRCmdList& cmdList) {
	auto* internalCmdList = to_dx12_internal(cmdList);

	const D3D12_RECT scissorRect = {
		.left = static_cast<LONG>(viewport.topLeftX),
		.top = static_cast<LONG>(viewport.topLeftY),
		.right = static_cast<LONG>(viewport.topLeftX + viewport.width),
		.bottom = static_cast<LONG>(viewport.topLeftY + viewport.height)
	};

	internalCmdList->graphicsCmdList->RSSetViewports(1, reinterpret_cast<const D3D12_VIEWPORT*>(&viewport));
	internalCmdList->graphicsCmdList->RSSetScissorRects(1, &scissorRect);
}

void SRGraphicsDevice_DX12::Impl::bind_vertex_buffer(const SRBuffer& buffer, const SRCmdList& cmdList) {
	assert(has_flag(buffer.info.bindFlags, SRBindFlag::VertexBuffer));
	auto* internalBuffer = to_dx12_internal(buffer);
	auto* internalCmdList = to_dx12_internal(cmdList);

	const D3D12_VERTEX_BUFFER_VIEW vertexBufferView = {
		.BufferLocation = internalBuffer->allocation->GetResource()->GetGPUVirtualAddress(),
		.SizeInBytes = static_cast<UINT>(buffer.info.size),
		.StrideInBytes = buffer.info.stride
	};

	internalCmdList->graphicsCmdList->IASetVertexBuffers(0, 1, &vertexBufferView);
}

void SRGraphicsDevice_DX12::Impl::bind_index_buffer(const SRBuffer& buffer, const SRCmdList& cmdList) {
	assert(has_flag(buffer.info.bindFlags, SRBindFlag::IndexBuffer));
	auto* internalBuffer = to_dx12_internal(buffer);
	auto* internalCmdList = to_dx12_internal(cmdList);

	const D3D12_INDEX_BUFFER_VIEW indexBufferView = {
		.BufferLocation = internalBuffer->allocation->GetResource()->GetGPUVirtualAddress(),
		.SizeInBytes = static_cast<UINT>(buffer.info.size),
		.Format = DXGI_FORMAT_R32_UINT
	};

	internalCmdList->graphicsCmdList->IASetIndexBuffer(&indexBufferView);
}

void SRGraphicsDevice_DX12::Impl::bind_root_constant_buffer(const SRBuffer& buffer, const SRCmdList& cmdList) {
	assert(has_flag(buffer.info.bindFlags, SRBindFlag::ConstantBuffer));
	auto* internalBuffer = to_dx12_internal(buffer);
	auto* internalCmdList = to_dx12_internal(cmdList);

	internalCmdList->graphicsCmdList->SetGraphicsRootConstantBufferView(
		1,
		internalBuffer->allocation->GetResource()->GetGPUVirtualAddress()
	);
}

void SRGraphicsDevice_DX12::Impl::push_constants(const void* data, u32 size, const SRCmdList& cmdList) {
	auto* internalCmdList = to_dx12_internal(cmdList);
	assert(size <= 128);

	internalCmdList->graphicsCmdList->SetGraphicsRoot32BitConstants(
		0,
		size >> 2,
		data,
		0
	);
}

void SRGraphicsDevice_DX12::Impl::barrier(const SRBarrier* pBarriers, u32 numBarriers, const SRCmdList& cmdList) {
	if (!pBarriers || numBarriers <= 0) {
		return;
	}

	// TODO: Support UAV and buffer barriers
	auto* internalCmdList = to_dx12_internal(cmdList);
	std::vector<D3D12_TEXTURE_BARRIER> dx12Barriers;
	dx12Barriers.reserve(numBarriers);

	for (u32 i = 0; i < numBarriers; ++i) {
		const SRBarrier& barrier = pBarriers[i];
		auto* internalTexture = to_dx12_internal(*barrier.image.texture);

		D3D12_TEXTURE_BARRIER dx12Barrier = {
			.SyncBefore = to_dx12_pipeline_stage(barrier.image.syncBefore),
			.SyncAfter = to_dx12_pipeline_stage(barrier.image.syncAfter),
			.AccessBefore = to_dx12_access_mask(barrier.image.accessBefore),
			.AccessAfter = to_dx12_access_mask(barrier.image.accessAfter),
			.LayoutBefore = to_dx12_resource_state(barrier.image.stateBefore),
			.LayoutAfter = to_dx12_resource_state(barrier.image.stateAfter),
			.pResource = internalTexture->allocation->GetResource(),
			.Subresources = { 0xffffffff, 0, 0, 0, 0, 0 },
			.Flags = D3D12_TEXTURE_BARRIER_FLAG_NONE
		};

		if (barrier.image.stateBefore == SRResourceState::Undefined) {
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

SRCmdList SRGraphicsDevice_DX12::Impl::begin_command_list(SRQueue queue) {
	size_t& cmdListCounter = m_PerFrameCmdListCounters[m_FrameIndex];
	auto& cmdLists = m_PerFrameCmdLists[m_FrameIndex];

	if (cmdListCounter >= cmdLists.size()) {
		cmdLists.push_back(std::make_unique<SRCmdList_DX12>());
	}

	auto internalCmdList = cmdLists[cmdListCounter].get();
	if (internalCmdList->graphicsCmdList == nullptr) {
		// NOTE: We require ID3D12GraphicsCommandList7 to be available
		SR_DX12_CHECK(m_Device->CreateCommandList(
			0,
			to_dx12_cmd_list_type(queue),
			m_CommandAllocators[queue][m_FrameIndex].Get(),
			nullptr,
			IID_PPV_ARGS(&internalCmdList->graphicsCmdList)
		), "Command list creation");

		// All command lists begin in recording state, so we close it
		internalCmdList->graphicsCmdList->Close();
	}

	SR_DX12_CHECK(m_CommandAllocators[queue][m_FrameIndex]->Reset(), "Reset command allocator");
	SR_DX12_CHECK(internalCmdList->graphicsCmdList->Reset(
		m_CommandAllocators[queue][m_FrameIndex].Get(),
		nullptr
	), "Begin command list recording");

	ID3D12DescriptorHeap* const descriptorHeaps[] = {
		m_ResourceDescriptorHeap.get_heap_object(),
		m_SamplerDescriptorHeap.get_heap_object()
	};
	internalCmdList->graphicsCmdList->SetDescriptorHeaps(std::size(descriptorHeaps), descriptorHeaps);

	++cmdListCounter;
	return SRCmdList{ internalCmdList };
}

void SRGraphicsDevice_DX12::Impl::begin_render_pass(const SRSwapchain& swapchain, const SRCmdList& cmdList) {
	auto internalSwapchain = to_dx12_internal(swapchain);
	auto internalCmdList = to_dx12_internal(cmdList);
	m_ImageIndex = internalSwapchain->swapchain->GetCurrentBackBufferIndex();

	// NOTE: Stingray always assumes that the swapchain will never be cleared,
	// and thus we assume an immediate overwrite of the swapchain backbuffer.
	// Hence why D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD is used. The
	// reason for this is that swapchain clears are notoriously slow to perform,
	// and thus it was decided to not allow such clears.
	const D3D12_CLEAR_VALUE clearValue = {
		.Format = to_dx12_format(swapchain.info.format),
		.Color = { 0.0F, 0.0F, 0.0F, 1.0F }
	};

	const D3D12_RENDER_PASS_RENDER_TARGET_DESC passRTVDesc = {
		.cpuDescriptor = m_RTVDescriptorHeap.get_cpu_handle(internalSwapchain->rtvDescriptors[m_ImageIndex]),
		.BeginningAccess = {
			.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR,
			.Clear = { .ClearValue = clearValue }
		},
		.EndingAccess = {
			.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE
		}
	};

	// TODO: Transition layout
	const SRImageTransitionInfo_DX12 transitionInfo = {
		.image = internalSwapchain->images[m_ImageIndex].Get(),
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

void SRGraphicsDevice_DX12::Impl::begin_render_pass(const SRPassInfo& passInfo, const SRCmdList& cmdList) {
	auto* internalCmdList = to_dx12_internal(cmdList);

	// RTVs
	std::vector<D3D12_RENDER_PASS_RENDER_TARGET_DESC> passRTVDescs;
	D3D12_RENDER_PASS_DEPTH_STENCIL_DESC passDSVDesc = {};
	passRTVDescs.reserve(passInfo.numColorAttachments);

	for (u32 i = 0; i < passInfo.numColorAttachments; ++i) {
		const SRPassInfo::Attachment& attachment = passInfo.colorAttachments[i];
		auto internalTexture = to_dx12_internal(*attachment.texture);
		assert(internalTexture);

		D3D12_RENDER_PASS_RENDER_TARGET_DESC rtvDesc = {
			.cpuDescriptor = m_RTVDescriptorHeap.get_cpu_handle(internalTexture->rtvDescriptor),
		};

		if (attachment.loadOp == SRLoadOp::Clear) {
			rtvDesc.BeginningAccess.Clear.ClearValue = {
				.Format = to_dx12_format(attachment.texture->info.format),
				.Color = { 0.0F, 0.0F, 0.0F, 1.0F }
			};
		}

		rtvDesc.BeginningAccess.Type = to_dx12_load_op(attachment.loadOp);
		rtvDesc.EndingAccess.Type = to_dx12_store_op(attachment.storeOp);

		passRTVDescs.push_back(rtvDesc);
	}

	// DSV
	const bool hasDepthAttachment = passInfo.depthAttachment.texture != nullptr;
	bool isReadOnlyDepth = false;
	if (hasDepthAttachment) {
		const SRPassInfo::Attachment& depthAttachment = passInfo.depthAttachment;
		auto internalTexture = to_dx12_internal(*depthAttachment.texture);
		assert(internalTexture);

		if (depthAttachment.loadOp == SRLoadOp::Clear) {
			passDSVDesc.cpuDescriptor = m_DSVDescriptorHeap.get_cpu_handle(internalTexture->dsvDescriptor);
			passDSVDesc.DepthBeginningAccess.Clear.ClearValue = {
				.Format = to_dx12_format(depthAttachment.texture->info.format),
				.DepthStencil = {
					.Depth = depthAttachment.clearValue,
					.Stencil = 0
				}
			};
		}
		else if (depthAttachment.loadOp == SRLoadOp::Load) {
			passDSVDesc.cpuDescriptor = m_DSVDescriptorHeap.get_cpu_handle(internalTexture->dsvReadOnlyDescriptor);
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
		passInfo.numColorAttachments,
		passRTVDescs.data(),
		hasDepthAttachment ? &passDSVDesc : nullptr,
		isReadOnlyDepth ? D3D12_RENDER_PASS_FLAG_BIND_READ_ONLY_DEPTH : D3D12_RENDER_PASS_FLAG_NONE
	);
}

void SRGraphicsDevice_DX12::Impl::end_render_pass(const SRSwapchain& swapchain, const SRCmdList& cmdList) {
	auto* internalSwapchain = to_dx12_internal(swapchain);
	auto* internalCmdList = to_dx12_internal(cmdList);
	internalCmdList->graphicsCmdList->EndRenderPass();

	const SRImageTransitionInfo_DX12 transitionInfo = {
		.image = internalSwapchain->images[m_ImageIndex].Get(),
		.oldLayout = D3D12_BARRIER_LAYOUT_RENDER_TARGET,
		.newLayout = D3D12_BARRIER_LAYOUT_PRESENT,
		.srcAccessMask = D3D12_BARRIER_ACCESS_RENDER_TARGET,
		.dstAccessMask = D3D12_BARRIER_ACCESS_NO_ACCESS,
		.srcStageMask = D3D12_BARRIER_SYNC_RENDER_TARGET,
		.dstStageMask = D3D12_BARRIER_SYNC_NONE,
	};
	SRDX12Helpers::transition_image_layout(transitionInfo, internalCmdList->graphicsCmdList.Get());
}

void SRGraphicsDevice_DX12::Impl::end_render_pass(const SRCmdList& cmdList) {
	auto* internalCmdList = to_dx12_internal(cmdList);
	internalCmdList->graphicsCmdList->EndRenderPass();
}

void SRGraphicsDevice_DX12::Impl::submit_command_lists(const SRSwapchain& swapchain) {
	auto internalSwapchain = to_dx12_internal(swapchain);
	const u32 numSubmittedCmdLists = (u32)m_PerFrameCmdListCounters[m_FrameIndex];
	m_PerFrameCmdListCounters[m_FrameIndex] = 0ULL;

	std::vector<ID3D12CommandList*> cmdListsToSubmit;
	cmdListsToSubmit.reserve(numSubmittedCmdLists);
	for (u32 i = 0; i < numSubmittedCmdLists; ++i) {
		const SRCmdList_DX12* cmdList = m_PerFrameCmdLists[m_FrameIndex][i].get();
		SR_DX12_CHECK(cmdList->graphicsCmdList->Close(), "Close command list");
		cmdListsToSubmit.push_back(cmdList->graphicsCmdList.Get());
	}

	m_CommandQueues[SRQueue_Universal]->ExecuteCommandLists(
		numSubmittedCmdLists,
		cmdListsToSubmit.data()
	);

	SR_DX12_CHECK(m_CommandQueues[SRQueue_Universal]->Signal(
		m_FrameFences[SRQueue_Universal].Get(),
		m_NextGPUSignalValue
	), "Signal fence");

	const UINT syncInterval = swapchain.info.vSync ? 1 : 0;
	const UINT presentFlags = m_IsTearingSupported && !swapchain.info.vSync ? DXGI_PRESENT_ALLOW_TEARING : 0;
	internalSwapchain->swapchain->Present(syncInterval, presentFlags);

	m_FrameDoneValues[SRQueue_Universal][m_FrameIndex] = m_NextGPUSignalValue++;
	++m_FrameCounter;
	const u32 nextFrameIndex = (m_FrameIndex + 1) % FRAMES_IN_FLIGHT;

	// Await frame value
	if (m_FrameCounter >= FRAMES_IN_FLIGHT) {
		const u64 needed = m_FrameDoneValues[SRQueue_Universal][nextFrameIndex];
		const u64 current = m_FrameFences[SRQueue_Universal]->GetCompletedValue();

		if (current < needed) {
			SR_DX12_CHECK(m_FrameFences[SRQueue_Universal]->SetEventOnCompletion(needed, nullptr), "Wait for fence");
		}
	}

	m_FrameIndex = nextFrameIndex;
}

SRShaderPlatformInfo SRGraphicsDevice_DX12::Impl::get_shader_platform_info() {
	return m_ShaderPlatformInfo;
}

void SRGraphicsDevice_DX12::Impl::wait_for_gpu() {
	// TODO: Right now we only use universal queue, remember that when we add
	// dedicated compute/copy queue we also need to Signal those here.
	const u64 target = ++m_NextGPUSignalValue;
	SR_DX12_CHECK(m_CommandQueues[SRQueue_Universal]->Signal(m_FrameFences[SRQueue_Universal].Get(), target), "Signal fence");

	if (m_FrameFences[SRQueue_Universal]->GetCompletedValue() < target) {
		SR_DX12_CHECK(m_FrameFences[SRQueue_Universal]->SetEventOnCompletion(target, nullptr), "Wait for fence");
	}
}

void SRGraphicsDevice_DX12::Impl::flush_initial_uploads() {
	SR_DX12_CHECK(m_UploadCmdList->Close(), "Close command list");

	ID3D12CommandList* cmdLists[1] = { m_UploadCmdList.Get() };
	m_CommandQueues[SRQueue_Copy]->ExecuteCommandLists(
		1,
		cmdLists
	);

	// TEMPORARY
	ComPtr<ID3D12Fence> tempFence;
	SR_DX12_CHECK(m_Device->CreateFence(
		0,
		D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(&tempFence)
	), "Create temporary fence");

	SR_DX12_CHECK(m_CommandQueues[SRQueue_Copy]->Signal(tempFence.Get(), 1), "Signal fence");

	if (tempFence->GetCompletedValue() < 1) {
		SR_DX12_CHECK(tempFence->SetEventOnCompletion(1, nullptr), "Wait for fence");
	}

	m_PendingUploadResources.clear();
	m_UploadCmdAllocator->Reset();
}

void SRGraphicsDevice_DX12::Impl::draw(u32 vtxCount, u32 startVtx, const SRCmdList& cmdList) {
	auto* internalCmdList = to_dx12_internal(cmdList);
	internalCmdList->graphicsCmdList->DrawInstanced(vtxCount, 1, startVtx, 0);
}

void SRGraphicsDevice_DX12::Impl::draw_indexed(u32 idxCount, u32 startIdx, u32 baseVtx, const SRCmdList& cmdList) {
	auto* internalCmdList = to_dx12_internal(cmdList);
	internalCmdList->graphicsCmdList->DrawIndexedInstanced(
		idxCount,
		1,
		startIdx,
		baseVtx,
		0
	);
}

void SRGraphicsDevice_DX12::Impl::setup_imgui_init_info(SRFormat swapchainFormat) {
	ImGui_ImplDX12_InitInfo initInfo = {};
	initInfo.Device = m_Device.Get();
	initInfo.CommandQueue = m_CommandQueues[SRQueue_Universal].Get();
	initInfo.NumFramesInFlight = FRAMES_IN_FLIGHT;
	initInfo.RTVFormat = to_dx12_format(swapchainFormat);
	initInfo.UserData = &m_ResourceDescriptorHeap;
	initInfo.SrvDescriptorHeap = m_ResourceDescriptorHeap.get_heap_object();

	initInfo.SrvDescriptorAllocFn = [](
		ImGui_ImplDX12_InitInfo* initInfo,
		D3D12_CPU_DESCRIPTOR_HANDLE* cpuHandle,
		D3D12_GPU_DESCRIPTOR_HANDLE* gpuHandle
	) {
		SRDescriptorHeap_DX12* descriptorHeap = reinterpret_cast<SRDescriptorHeap_DX12*>(
			initInfo->UserData
		);

		const SRDescriptorIndex descriptorIndex = descriptorHeap->get_next_index();
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
		const SRDescriptorIndex descriptorIndex = descriptorHeap->get_index_from_handle(cpuHandle);
		descriptorHeap->free_index(descriptorIndex);
	};

	ImGui_ImplDX12_Init(&initInfo);
}

SRDescriptorIndex SRGraphicsDevice_DX12::Impl::get_descriptor_index_srv(const SRResource& resource) {
	assert(resource.type == SRResourceType::Texture); // TODO: Support other SRV types

	if (resource.type == SRResourceType::Texture) {
		auto* internalTexture = (SRTexture_DX12*)resource.internalState.get();
		return internalTexture->srvDescriptor;
	}
	if (resource.type == SRResourceType::Buffer) {
		auto* internalBuffer = (SRBuffer_DX12*)resource.internalState.get();
		return internalBuffer->srvDescriptor;
	}

	assert(false);
	return INVALID_DESCRIPTOR_INDEX;
}

void SRGraphicsDevice_DX12::Impl::dispatch_mesh(u32 groupCountX, u32 groupCountY, u32 groupCountZ, const SRCmdList& cmdList) {
	auto* internalCmdList = to_dx12_internal(cmdList);

	internalCmdList->graphicsCmdList->DispatchMesh(groupCountX, groupCountY, groupCountZ);
}

// --------------------------------- Public API --------------------------------
SRGraphicsDevice_DX12::SRGraphicsDevice_DX12(SRWindow& window) : SRGraphicsDevice(window) {
	m_Impl = new Impl(window);
	m_Impl->create_debug_interface();
	m_Impl->create_dxgi_debug_interface();
	m_Impl->create_dxgi_factory();
	m_Impl->create_device();
	m_Impl->create_memory_allocator();
	m_Impl->create_command_allocators();
	m_Impl->create_command_queues();
	m_Impl->create_sync_objects();
	m_Impl->create_descriptor_heaps();
}

SRGraphicsDevice_DX12::~SRGraphicsDevice_DX12() {
	delete m_Impl;
	m_Impl = nullptr;
}

u32 SRGraphicsDevice_DX12::get_frame_index() const {
	return m_Impl->m_FrameIndex;
}

void SRGraphicsDevice_DX12::create_swapchain(const SRSwapchainInfo& info, SRSwapchain& swapchain) {
	m_Impl->create_swapchain(info, swapchain);
}

void SRGraphicsDevice_DX12::create_pipeline(const SRPipelineInfo& info, SRPipeline& pipeline) {
	m_Impl->create_pipeline(info, pipeline);
}

void SRGraphicsDevice_DX12::create_buffer(const SRBufferInfo& info, SRBuffer& buffer, const void* data) {
	m_Impl->create_buffer(info, buffer, data);
}

void SRGraphicsDevice_DX12::create_texture(const SRTextureInfo& info, SRTexture& texture, const SRSubresourceData* data) {
	m_Impl->create_texture(info, texture, data);
}

void SRGraphicsDevice_DX12::create_sampler(const SRSamplerInfo& info, SRSampler& sampler) {
	m_Impl->create_sampler(info, sampler);
}

void SRGraphicsDevice_DX12::bind_pipeline(const SRPipeline& pipeline, const SRCmdList& cmdList) {
	m_Impl->bind_pipeline(pipeline, cmdList);
}

void SRGraphicsDevice_DX12::bind_viewport(const SRViewport& viewport, const SRCmdList& cmdList) {
	m_Impl->bind_viewport(viewport, cmdList);
}

void SRGraphicsDevice_DX12::bind_vertex_buffer(const SRBuffer& buffer, const SRCmdList& cmdList) {
	m_Impl->bind_vertex_buffer(buffer, cmdList);
}

void SRGraphicsDevice_DX12::bind_index_buffer(const SRBuffer& buffer, const SRCmdList& cmdList) {
	m_Impl->bind_index_buffer(buffer, cmdList);
}

void SRGraphicsDevice_DX12::bind_root_constant_buffer(const SRBuffer& buffer, const SRCmdList& cmdList) {
	m_Impl->bind_root_constant_buffer(buffer, cmdList);
}

void SRGraphicsDevice_DX12::push_constants(const void* data, u32 size, const SRCmdList& cmdList) {
	m_Impl->push_constants(data, size, cmdList);
}

void SRGraphicsDevice_DX12::barrier(const SRBarrier* pBarriers, u32 numBarriers, const SRCmdList& cmdList) {
	m_Impl->barrier(pBarriers, numBarriers, cmdList);
}

SRCmdList SRGraphicsDevice_DX12::begin_command_list(SRQueue queue) {
	return m_Impl->begin_command_list(queue);
}

void SRGraphicsDevice_DX12::begin_render_pass(const SRSwapchain& swapchain, const SRCmdList& cmdList) {
	m_Impl->begin_render_pass(swapchain, cmdList);
}

void SRGraphicsDevice_DX12::begin_render_pass(const SRPassInfo& passInfo, const SRCmdList& cmdList) {
	m_Impl->begin_render_pass(passInfo, cmdList);
}

void SRGraphicsDevice_DX12::end_render_pass(const SRSwapchain& swapchain, const SRCmdList& cmdList) {
	m_Impl->end_render_pass(swapchain, cmdList);
}

void SRGraphicsDevice_DX12::end_render_pass(const SRCmdList& cmdList) {
	m_Impl->end_render_pass(cmdList);
}

void SRGraphicsDevice_DX12::submit_command_lists(const SRSwapchain& swapchain) {
	m_Impl->submit_command_lists(swapchain);
}

void SRGraphicsDevice_DX12::draw(u32 vtxCount, u32 startVtx, const SRCmdList& cmdList) {
	m_Impl->draw(vtxCount, startVtx, cmdList);
}

void SRGraphicsDevice_DX12::draw_indexed(u32 idxCount, u32 startIdx, u32 baseVtx, const SRCmdList& cmdList) {
	m_Impl->draw_indexed(idxCount, startIdx, baseVtx, cmdList);
}

void SRGraphicsDevice_DX12::dispatch_mesh(u32 groupCountX, u32 groupCountY, u32 groupCountZ, const SRCmdList& cmdList) {
	m_Impl->dispatch_mesh(groupCountX, groupCountY, groupCountZ, cmdList);
}

SRDescriptorIndex SRGraphicsDevice_DX12::get_descriptor_index_srv(const SRResource& resource) {
	return m_Impl->get_descriptor_index_srv(resource);
}

SRShaderPlatformInfo SRGraphicsDevice_DX12::get_shader_platform_info() {
	return m_Impl->get_shader_platform_info();
}

void SRGraphicsDevice_DX12::wait_for_gpu() {
	m_Impl->wait_for_gpu();
}

void SRGraphicsDevice_DX12::flush_initial_uploads() {
	m_Impl->flush_initial_uploads();
}

void SRGraphicsDevice_DX12::setup_imgui_init_info(SRFormat swapchainFormat) {
	m_Impl->setup_imgui_init_info(swapchainFormat);
}
