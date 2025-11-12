#include <Windows.h>
#include "Core/Logger.hpp"
#include "Core/Window.hpp"
#include "Core/System/Time.hpp"
#include "Data/Camera.hpp"
#include "Data/Model.hpp"
#include "Graphics/FrameInfo.hpp"
#include "Graphics/GraphicsDevice.hpp"
#include "Graphics/RenderGraph.hpp"
#include "Graphics/DX12/GraphicsDevice_DX12.hpp"
#include "Graphics/Vulkan/GraphicsDevice_Vulkan.hpp"
#include "Graphics/Renderpasses/TestingPass.hpp"
#include "Graphics/ShaderCompiler.hpp"
#include "Input/Input.hpp"
#include "UI/Editor.hpp"

#include <glm/glm.hpp>
#include <cassert>
#include <iostream>
#include <memory>
#include <vector>

struct alignas(256) PerFrameData {
	glm::mat4 view = { 1.0f };
	glm::mat4 proj = { 1.0f };
	glm::mat4 invView = { 1.0f };
	glm::mat4 invProj = { 1.0f };
};

// NOTE: Trick for making sure that the logger exists longer than all other objects
static auto& logger = SRLogger::get();
static constexpr SRGraphicsAPI g_API = SRGraphicsAPI::VULKAN;
static constexpr int WIDTH = 1920;
static constexpr int HEIGHT = 1080;

std::unique_ptr<SRWindow> g_Window = {};
std::unique_ptr<SRGraphicsDevice> g_GfxDevice = {};
std::unique_ptr<SRRenderGraph> g_RenderGraph = {};
std::unique_ptr<SRShaderCompiler> g_ShaderCompiler = {};
std::unique_ptr<SREditor> g_Editor = {};
std::unique_ptr<SRCamera> g_Camera = {};

SRBuffer g_VertexBuffer = {};
SRBuffer g_PerFrameBuffers[SRGraphicsDevice::FRAMES_IN_FLIGHT] = {};
PerFrameData g_PerFrameData = {};
SRSwapchain g_Swapchain = {};

void init_console();
void init_resources();
void init_rendergraph();
void update(const SRFrameInfo& frameInfo);
void render(const SRFrameInfo& frameInfo);

int main() {
	init_console();

	const char* windowTitle = (g_API == SRGraphicsAPI::VULKAN ? "Stingray (Vulkan)" : "Stingray (DX12)");
	g_Window = std::make_unique<SRWindow>(windowTitle, WIDTH, HEIGHT, SRWindowFlags_Centered | SRWindowFlags_SizeIsClientArea);
	SRInput::initialize(g_Window.get());

	if (g_API == SRGraphicsAPI::VULKAN) {
		g_GfxDevice = std::make_unique<SRGraphicsDevice_Vulkan>(*g_Window);
	}
	else if (g_API == SRGraphicsAPI::DX12) {
		g_GfxDevice = std::make_unique<SRGraphicsDevice_DX12>(*g_Window);
	}
	g_ShaderCompiler = std::make_unique<SRShaderCompiler>(g_GfxDevice->get_shader_platform_info());

	init_resources();
	init_rendergraph();
	g_GfxDevice->flush_initial_uploads(); // TEMPORARY but important for now

	g_Camera = std::make_unique<SRCamera>(
		glm::vec3(0.0f, 0.0f, -2.0f),
		glm::angleAxis(glm::radians(0.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
		60.0f,
		g_Window->get_client_aspect_ratio(),
		0.1f,
		20.0f
	);

	const SRSwapchainInfo swapchainInfo = {
		.width = WIDTH,
		.height = HEIGHT,
		.numBuffers = 3,
		.format = SRFormat::BGRA8_UNORM,
		.vSync = true
	};
	g_GfxDevice->create_swapchain(swapchainInfo, g_Swapchain);
	g_Editor = std::make_unique<SREditor>(*g_Window, *g_GfxDevice, g_API);

	bool firstFrame = true;
	SRFrameInfo frameInfo = {
		.camera = g_Camera.get(),
		.dt = 0.0f
	};

	SRTime::initialize();
	while (g_Window->poll_events()) {
		SRTime::begin_frame();
		frameInfo.perFrameBuffer = &g_PerFrameBuffers[g_GfxDevice->get_frame_index()];
		frameInfo.dt = SRTime::get_delta_sec();
		frameInfo.width = WIDTH;
		frameInfo.height = HEIGHT;

		update(frameInfo);
		render(frameInfo);

		if (firstFrame) {
			g_Window->show();
			firstFrame = false;
		}
	}

	g_GfxDevice->wait_for_gpu();

	return 0;
}

void init_console() {
	HANDLE handleOut = GetStdHandle(STD_OUTPUT_HANDLE);
	DWORD consoleMode;
	GetConsoleMode(handleOut, &consoleMode);
	consoleMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
	consoleMode |= DISABLE_NEWLINE_AUTO_RETURN;
	SetConsoleMode(handleOut, consoleMode);
}

void init_resources() {
	const SRBufferInfo perFrameBufferInfo = {
		.size = sizeof(PerFrameData),
		.stride = sizeof(PerFrameData),
		.usage = SRUsage::UPLOAD,
		.bindFlags = SRBindFlag_ConstantBuffer
	};

	for (uint32_t f = 0; f < SRGraphicsDevice::FRAMES_IN_FLIGHT; ++f) {
		g_GfxDevice->create_buffer(perFrameBufferInfo, g_PerFrameBuffers[f], &g_PerFrameData);
	}
}

void init_rendergraph() {
	g_RenderGraph = std::make_unique<SRRenderGraph>();

	auto& testingPass = g_RenderGraph->add_render_pass("CompositionPass", SRPassType::GRAPHICS)
		.set_execute_callback(SRTestingPass::execute);
	SRTestingPass::build(testingPass, *g_GfxDevice, *g_ShaderCompiler);

	auto& imguiPass = g_RenderGraph->add_render_pass("ImGuiPass", SRPassType::GRAPHICS)
		.set_execute_callback([](SRRenderPass& self, SRGraphicsDevice& gfxDevice, const SRCmdList& cmdList, const SRFrameInfo& frameInfo) {
			g_Editor->update();
			g_Editor->render(cmdList);
		});

	g_RenderGraph->build(*g_GfxDevice);
}

void update(const SRFrameInfo& frameInfo) {
	SRInput::update();
	SRMouseState mouse = SRInput::get_mouse_state();
	SRLOG_TRACE("Mouse Delta: %d, %d", mouse.dx, mouse.dy);

	const float cameraMoveSpeed = 4.0f;
	const float mouseSensitivity = 0.001f;
	const float dx = mouseSensitivity * mouse.dx;
	const float dy = mouseSensitivity * mouse.dy;

	glm::quat orientation = g_Camera->get_orientation();
	orientation = orientation * glm::angleAxis(dy, glm::vec3(1.0f, 0.0f, 0.0f)); // pitch
	orientation = glm::angleAxis(dx, glm::vec3(0.0f, 1.0f, 0.0f)) * orientation; // yaw
	g_Camera->set_orientation(orientation);

	const glm::vec3 camRight = g_Camera->get_right();
	const glm::vec3 camUp = g_Camera->get_up();
	const glm::vec3 camForward = g_Camera->get_forward();
	glm::vec3 newPosition = g_Camera->get_position();
	if (SRInput::is_key_down(SRKey_W)) {
		newPosition += cameraMoveSpeed * frameInfo.dt * camForward;
	}
	if (SRInput::is_key_down(SRKey_A)) {
		newPosition -= cameraMoveSpeed * frameInfo.dt * camRight;
	}
	if (SRInput::is_key_down(SRKey_S)) {
		newPosition -= cameraMoveSpeed * frameInfo.dt * camForward;
	}
	if (SRInput::is_key_down(SRKey_D)) {
		newPosition += cameraMoveSpeed * frameInfo.dt * camRight;
	}
	g_Camera->set_position(newPosition);

	g_Camera->update();
	g_PerFrameData.view = g_Camera->get_view_matrix();
	g_PerFrameData.proj = g_Camera->get_proj_matrix();

	std::memcpy(g_PerFrameBuffers[g_GfxDevice->get_frame_index()].mappedData, &g_PerFrameData, sizeof(g_PerFrameData));
}

void render(const SRFrameInfo& frameInfo) {
	const SRCmdList cmdList = g_GfxDevice->begin_command_list(SRQueue_Universal);
	g_RenderGraph->execute(*g_GfxDevice, g_Swapchain, cmdList, frameInfo);
	g_GfxDevice->submit_command_lists(g_Swapchain);
}
