#include "GraphicsDevice_DX12.hpp"
#include "Graphics/DX12/GraphicsHelpers_DX12.hpp"
#include "Graphics/DX12/GraphicsTypes_DX12.hpp"
#include "Core/Logger.hpp"
#include "Utilities/TextUtilities.hpp"

#include "AgilitySDK/d3d12.h"
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
	void create_command_allocators();
	void create_command_queues();
	void create_sync_objects();
	void create_descriptor_heaps();

	void create_swapchain(const SRSwapchainInfo& info, SRSwapchain& swapchain);
	void create_pipeline(const SRPipelineInfo& info, SRPipeline& pipeline);
	void bind_viewport(const SRViewport& viewport, const SRCmdList& cmdList);

	SRCmdList begin_command_list(SRQueue queue);
	void begin_render_pass(const SRSwapchain& swapchain, const SRCmdList& cmdList);
	void end_render_pass(const SRSwapchain& swapchain, const SRCmdList& cmdList);
	void submit_command_lists(const SRSwapchain& swapchain);

	void wait_for_gpu();

	SRWindow& m_Window;
	#ifdef _DEBUG
	ComPtr<ID3D12Debug> m_DebugInterface;
	ComPtr<IDXGIInfoQueue> m_DXGIDebugInfoQueue;
	ComPtr<ID3D12DebugDevice> m_DebugDevice;
	#endif

	ComPtr<IDXGIFactory2> m_DXGIFactory;
	ComPtr<ID3D12Device> m_Device;
	SRDeviceCapabilities_DX12 m_DeviceCapabilities = {};
	ComPtr<ID3D12CommandAllocator> m_CommandAllocators[SRQueue_COUNT][FRAMES_IN_FLIGHT];
	ComPtr<ID3D12CommandQueue> m_CommandQueues[SRQueue_COUNT];

	SRDescriptorHeap_DX12 m_ResourceDescriptorHeap = { D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, MAX_RESOURCE_DESCRIPTORS }; // CBV + SRV + UAV
	SRDescriptorHeap_DX12 m_SamplerDescriptorHeap = { D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, MAX_SAMPLER_DESCRIPTORS };
	SRDescriptorHeap_DX12 m_RTVDescriptorHeap = { D3D12_DESCRIPTOR_HEAP_TYPE_RTV, MAX_RTV_DESCRIPTORS };
	SRDescriptorHeap_DX12 m_DSVDescriptorHeap = { D3D12_DESCRIPTOR_HEAP_TYPE_DSV, MAX_DSV_DESCRIPTORS };

	ComPtr<ID3D12Fence> m_FrameFences[SRQueue_COUNT];
	uint64_t m_FrameDoneValues[SRQueue_COUNT][FRAMES_IN_FLIGHT] = {};
	uint64_t m_NextGPUSignalValue = 1;
	bool m_IsFirstCmdListThisFrame[SRQueue_COUNT][FRAMES_IN_FLIGHT] = {};

	std::vector<std::unique_ptr<SRCmdList_DX12>> m_PerFrameCmdLists[FRAMES_IN_FLIGHT];
	size_t m_PerFrameCmdListCounters[FRAMES_IN_FLIGHT] = {};
	uint32_t m_FrameIndex = 0;
	uint32_t m_ImageIndex = 0;
	uint64_t m_FrameCounter = 0;
	bool m_IsTearingSupported = false;

	static constexpr uint32_t MAX_RESOURCE_DESCRIPTORS = 32768;
	static constexpr uint32_t MAX_SAMPLER_DESCRIPTORS = 16;
	static constexpr uint32_t MAX_RTV_DESCRIPTORS = 256;
	static constexpr uint32_t MAX_DSV_DESCRIPTORS = 32;
};

// -------------------------- Impl Method Definitions --------------------------
SRGraphicsDevice_DX12::Impl::~Impl() {
	
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
	uint32_t pickedDeviceIdx = ~0;
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

		m_Device = device;

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

void SRGraphicsDevice_DX12::Impl::create_command_allocators() {
	for (uint32_t f = 0; f < FRAMES_IN_FLIGHT; ++f) {
		SR_DX12_CHECK(m_Device->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(&m_CommandAllocators[SRQueue_Universal][f])
		), "Command allocator creation");
	}

	// TODO: Create command allocators for other queue types too. We will need
	// it eventually.
}

void SRGraphicsDevice_DX12::Impl::create_command_queues() {
	const D3D12_COMMAND_QUEUE_DESC commandQueueDesc = {
		.Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
		.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
		.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
		.NodeMask = 0
	};

	SR_DX12_CHECK(m_Device->CreateCommandQueue(
		&commandQueueDesc,
		IID_PPV_ARGS(&m_CommandQueues[SRQueue_Universal])
	), "Command queue creation");

	// TODO: Create command queues for other queue types too. We will need it
	// eventually.
}


void SRGraphicsDevice_DX12::Impl::create_sync_objects() {
	for (uint32_t q = 0; q < SRQueue_COUNT; ++q) {
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

	// TODO: Store this backbuffer index into m_ImageIndex
	const UINT bufferIndex = internalSwapchain->swapchain->GetCurrentBackBufferIndex();
	internalSwapchain->images.resize(info.numBuffers);
	internalSwapchain->rtvDescriptors.reserve(info.numBuffers);

	for (uint32_t i = 0; i < info.numBuffers; ++i) {
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

}

void SRGraphicsDevice_DX12::Impl::bind_viewport(const SRViewport& viewport, const SRCmdList& cmdList) {
	auto internalCmdList = to_internal(cmdList);

	const D3D12_RECT scissorRect = {
		.left = static_cast<LONG>(viewport.topLeftX),
		.top = static_cast<LONG>(viewport.topLeftY),
		.right = static_cast<LONG>(viewport.topLeftX + viewport.width),
		.bottom = static_cast<LONG>(viewport.topLeftY + viewport.height)
	};

	internalCmdList->graphicsCmdList->RSSetViewports(1, reinterpret_cast<const D3D12_VIEWPORT*>(&viewport));
	internalCmdList->graphicsCmdList->RSSetScissorRects(1, &scissorRect);
}

SRCmdList SRGraphicsDevice_DX12::Impl::begin_command_list(SRQueue queue) {
	size_t& cmdListCounter = m_PerFrameCmdListCounters[m_FrameIndex];
	auto& cmdLists = m_PerFrameCmdLists[m_FrameIndex];

	if (cmdListCounter >= cmdLists.size()) {
		cmdLists.push_back(std::make_unique<SRCmdList_DX12>());
	}

	auto internalCmdList = cmdLists[cmdListCounter].get();
	if (internalCmdList->graphicsCmdList == nullptr) {
		// NOTE: We require ID3D12GraphicsCommandList6 to be available
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

	// Wait until cmdList is done processing on GPU, only then is it safe to
	// reset the command allocator
	// Reset the command pool JUST BEFORE we begin command buffer recording.
	// This results in as little potential CPU waiting as possible.
	// Should only be done ONCE per frame per queue family.
	if (m_IsFirstCmdListThisFrame[queue][m_FrameIndex] && m_FrameCounter >= FRAMES_IN_FLIGHT) {
		const uint64_t needed = m_FrameDoneValues[queue][m_FrameIndex];
		const uint64_t current = m_FrameFences[queue]->GetCompletedValue();

		if (current < needed) {
			SR_DX12_CHECK(m_FrameFences[queue]->SetEventOnCompletion(needed, nullptr), "Wait for fence");
		}

		SR_DX12_CHECK(m_CommandAllocators[queue][m_FrameIndex]->Reset(), "Reset command allocator");
		m_IsFirstCmdListThisFrame[queue][m_FrameIndex] = false;
	}
	SR_DX12_CHECK(internalCmdList->graphicsCmdList->Reset(
		m_CommandAllocators[queue][m_FrameIndex].Get(),
		nullptr
	), "Begin command list recording");

	++cmdListCounter;
	return SRCmdList{ internalCmdList };
}

void SRGraphicsDevice_DX12::Impl::begin_render_pass(const SRSwapchain& swapchain, const SRCmdList& cmdList) {
	auto internalSwapchain = to_internal(swapchain);
	auto internalCmdList = to_internal(cmdList);
	m_ImageIndex = internalSwapchain->swapchain->GetCurrentBackBufferIndex();

	// NOTE: Stingray always assumes that the swapchain will never be cleared,
	// and thus we assume an immediate overwrite of the swapchain backbuffer.
	// Hence why D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD is used. The
	// reason for this is that swapchain clears are notoriously slow to perform,
	// and thus it was decided to not allow such clears.
	const D3D12_CLEAR_VALUE clearValue = {
		.Format = to_dx12_format(swapchain.info.format),
		.Color = { 0.0F, 1.0F, 0.0F, 1.0F }
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

void SRGraphicsDevice_DX12::Impl::end_render_pass(const SRSwapchain& swapchain, const SRCmdList& cmdList) {
	auto internalSwapchain = to_internal(swapchain);
	auto internalCmdList = to_internal(cmdList);
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

void SRGraphicsDevice_DX12::Impl::submit_command_lists(const SRSwapchain& swapchain) {
	auto internalSwapchain = to_internal(swapchain);
	const uint32_t numSubmittedCmdLists = m_PerFrameCmdListCounters[m_FrameIndex];
	m_PerFrameCmdListCounters[m_FrameIndex] = 0;

	std::vector<ID3D12CommandList*> cmdListsToSubmit;
	cmdListsToSubmit.reserve(numSubmittedCmdLists);
	for (uint32_t i = 0; i < numSubmittedCmdLists; ++i) {
		const SRCmdList_DX12* cmdList = m_PerFrameCmdLists[m_FrameIndex][i].get();
		cmdList->graphicsCmdList->Close();
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
	m_FrameIndex = (m_FrameIndex + 1) % FRAMES_IN_FLIGHT;
	for (uint32_t q = 0; q < SRQueue_COUNT; ++q) {
		m_IsFirstCmdListThisFrame[q][m_FrameIndex] = true;
	}
}

void SRGraphicsDevice_DX12::Impl::wait_for_gpu() {
	// TODO: Right now we only use universal queue, remember that when we add
	// dedicated compute/copy queue we also need to Signal those here.
	const uint64_t target = ++m_NextGPUSignalValue;
	SR_DX12_CHECK(m_CommandQueues[SRQueue_Universal]->Signal(m_FrameFences[SRQueue_Universal].Get(), target), "Signal fence");

	if (m_FrameFences[SRQueue_Universal]->GetCompletedValue() < target) {
		SR_DX12_CHECK(m_FrameFences[SRQueue_Universal]->SetEventOnCompletion(target, nullptr), "Wait for fence");
	}
}

// --------------------------------- Public API --------------------------------
SRGraphicsDevice_DX12::SRGraphicsDevice_DX12(SRWindow& window) : SRGraphicsDevice(window) {
	m_Impl = new Impl(window);
	m_Impl->create_debug_interface();
	m_Impl->create_dxgi_debug_interface();
	m_Impl->create_dxgi_factory();
	m_Impl->create_device();
	m_Impl->create_command_allocators();
	m_Impl->create_command_queues();
	m_Impl->create_sync_objects();
	m_Impl->create_descriptor_heaps();
}

SRGraphicsDevice_DX12::~SRGraphicsDevice_DX12() {
	delete m_Impl;
	m_Impl = nullptr;
}

uint32_t SRGraphicsDevice_DX12::get_frame_index() const {
	return m_Impl->m_FrameIndex;
}

void SRGraphicsDevice_DX12::create_swapchain(const SRSwapchainInfo& info, SRSwapchain& swapchain) {
	m_Impl->create_swapchain(info, swapchain);
}

void SRGraphicsDevice_DX12::create_pipeline(const SRPipelineInfo& info, SRPipeline& pipeline) {
	m_Impl->create_pipeline(info, pipeline);
}

void SRGraphicsDevice_DX12::create_buffer(const SRBufferInfo& info, SRBuffer& buffer, const void* data) {

}

void SRGraphicsDevice_DX12::bind_pipeline(const SRPipeline& pipeline, const SRCmdList& cmdList) {

}

void SRGraphicsDevice_DX12::bind_viewport(const SRViewport& viewport, const SRCmdList& cmdList) {
	m_Impl->bind_viewport(viewport, cmdList);
}

void SRGraphicsDevice_DX12::bind_root_constant_buffer(const SRBuffer& buffer, const SRCmdList& cmdList) {

}

SRCmdList SRGraphicsDevice_DX12::begin_command_list(SRQueue queue) {
	return m_Impl->begin_command_list(queue);
}

void SRGraphicsDevice_DX12::begin_render_pass(const SRSwapchain& swapchain, const SRCmdList& cmdList) {
	m_Impl->begin_render_pass(swapchain, cmdList);
}

void SRGraphicsDevice_DX12::end_render_pass(const SRSwapchain& swapchain, const SRCmdList& cmdList) {
	m_Impl->end_render_pass(swapchain, cmdList);
}

void SRGraphicsDevice_DX12::submit_command_lists(const SRSwapchain& swapchain) {
	m_Impl->submit_command_lists(swapchain);
}

void SRGraphicsDevice_DX12::draw(uint32_t vtxCount, uint32_t startVtx, const SRCmdList& cmdList) {

}

SRShaderPlatformInfo SRGraphicsDevice_DX12::get_shader_platform_info() {
	return {};
}

void SRGraphicsDevice_DX12::wait_for_gpu() {
	m_Impl->wait_for_gpu();
}

void SRGraphicsDevice_DX12::flush_initial_uploads() {

}

