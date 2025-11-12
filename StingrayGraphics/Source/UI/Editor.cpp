#include "Editor.hpp"
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
	case SRGraphicsAPI::VULKAN:
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
	gfxDevice.setup_imgui_init_info(SRFormat::BGRA8_UNORM);
}

SREditor::~SREditor() {
	shutdown_func();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void SREditor::update() {
	begin_render_func();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	ImGui::ShowDemoWindow();
}

void SREditor::render(const SRCmdList& cmdList) {
	ImGui::Render();
	end_render_func(cmdList);
}
