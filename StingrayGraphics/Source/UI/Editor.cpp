#include "Editor.hpp"
#include "Core/System/Time.hpp"
#include "Graphics/DX12/GraphicsTypes_DX12.hpp"
#include "Graphics/Vulkan/GraphicsTypes_Vulkan.hpp"

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_vulkan.h>

namespace {
	void begin_render_dx12() {
		ImGui_ImplDX12_NewFrame();
	}

	void end_render_dx12(const SRCmdList& cmdList) {
		auto internalCmdList = to_dx12_internal(cmdList);
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), internalCmdList->graphicsCmdList.Get());
	}

	void begin_render_vulkan() {
		ImGui_ImplVulkan_NewFrame();
	}

	void end_render_vulkan(const SRCmdList& cmdList) {
		auto internalCmdList = to_vk_internal(cmdList);
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), internalCmdList->cmdBuffer);
	}
}

SREditor::SREditor(SRWindow& window, SRGraphicsDevice& gfxDevice, SRGraphicsAPI api) :
	m_Window(window), m_GfxDevice(gfxDevice), m_API(api) {

	switch (m_API) {
	case SRGraphicsAPI::DX12:
		begin_render_func = begin_render_dx12;
		end_render_func = end_render_dx12;
		shutdown_func = ImGui_ImplDX12_Shutdown;
		break;
	case SRGraphicsAPI::Vulkan:
		begin_render_func = begin_render_vulkan;
		end_render_func = end_render_vulkan;
		shutdown_func = ImGui_ImplVulkan_Shutdown;
		break;
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	ImGui_ImplWin32_Init(window.get_internal_handle());
	// TODO: Make format dynamic
	gfxDevice.setup_imgui_init_info(SRFormat::RGBA8_UNORM);
}

SREditor::~SREditor() {
	shutdown_func();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void SREditor::update(SRRenderGraph& renderGraph) {
	begin_render_func();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	ImGui::Begin("Properties");
	{
		// Update FPS counter
		static uint64_t totalFrames = 0;
		static double totalFrameTime = 0.0;
		static double accumulatedTime = 0.0;
		static uint64_t frameCount = 0;
		static uint64_t fps = 0;

		const double deltaTime = SRTime::get_delta_sec();
		totalFrameTime += deltaTime;
		accumulatedTime += deltaTime;
		++totalFrames;
		++frameCount;

		if (accumulatedTime >= 1.0) {
			const double fpsExact = frameCount / accumulatedTime;
			fps = static_cast<uint64_t>(fpsExact + 0.5);
			accumulatedTime = 0.0;
			frameCount = 0;
		}
		const double avgFrameTime = totalFrameTime / totalFrames;

		ImGui::SeparatorText("Performance Metrics");
		ImGui::Text("FPS: %llu", fps);
		ImGui::Text("Frame Time: %.2f ms", deltaTime * 1000.0);
		ImGui::Text("Avg Frame Time: %.2f ms", avgFrameTime * 1000.0);

		ImGui::SeparatorText("Shadows");

		//const auto renderPasses = renderGraph.GetAllPasses();

		//if (ImGui::BeginListBox("Renderpasses")) {
		//	LOCAL_PERSIST size_t selectedPassIdx = 0;
		//	LOCAL_PERSIST size_t highlightedPassIdx = 0;

		//	for (size_t i = 0; i < renderPasses.size(); ++i) {
		//		const bool isSelected = (selectedPassIdx == i);

		//		if (ImGui::Selectable(renderPasses[i]->GetName().c_str(), isSelected)) {
		//			selectedPassIdx = i;
		//		}

		//		if (ImGui::IsItemHovered()) {
		//			highlightedPassIdx = i;
		//		}

		//		// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
		//		if (isSelected) {
		//			ImGui::SetItemDefaultFocus();
		//		}
		//	}
		//	ImGui::EndListBox();
		//}

		//auto* vsmAttachment = renderGraph.GetAttachment("ShadowMap");
		//ImGui::Image(
		//	m_GfxDevice.GetGPUDescriptorPointer(
		//		vsmAttachment->texture,
		//		SubresourceType::SRV
		//	),
		//	{ 256, 256 }
		//);
	}
	ImGui::End();
}

void SREditor::render(const SRCmdList& cmdList) {
	ImGui::Render();
	end_render_func(cmdList);
}
