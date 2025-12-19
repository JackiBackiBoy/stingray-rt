#include "Core/Logger.h"
#include "Core/Window.h"
#include "Core/System/Time.h"
#include "Data/Camera.h"
#include "Data/ComponentTypes.h"
#include "Data/Model.h"
#include "Data/Scene.h"
#include "Graphics/FrameInfo.h"
#include "Graphics/GraphicsDevice.h"
#include "Graphics/RenderGraph.h"
#include "Graphics/DX12/GraphicsDevice_DX12.h"
#include "Graphics/Vulkan/GraphicsDevice_Vulkan.h"
#include "Graphics/Renderpasses/DepthPrepass.h"
#include "Graphics/Renderpasses/GBufferPass.h"
#include "Graphics/Renderpasses/CompositionPass.h"
#include "Graphics/Renderpasses/MeshletGenerationPass.h"
#include "Graphics/ShaderCompiler.h"
#include "Input/Input.h"
#include "UI/Editor.h"

#include <Windows.h>
#include <glm/glm.hpp>
#include <cassert>
#include <iostream>
#include <memory>
#include <vector>

struct alignas(256) PerFrameData {
	glm::mat4 view    = { 1.0f };
	glm::mat4 proj    = { 1.0f };
	glm::mat4 invView = { 1.0f };
	glm::mat4 invProj = { 1.0f };
};

// NOTE: Trick for making sure that the logger exists longer than all other objects
static auto& logger = SRLogger::get();
static constexpr SRGraphicsAPI g_API = SRGraphicsAPI::DX12;
static constexpr int WIDTH = 1920;
static constexpr int HEIGHT = 1080;

std::unique_ptr<SRWindow> g_Window = {};
std::unique_ptr<SRGraphicsDevice> g_GfxDevice = {};
std::unique_ptr<SRRenderGraph> g_RenderGraph = {};
std::unique_ptr<SRShaderCompiler> g_ShaderCompiler = {};
std::unique_ptr<SREditor> g_Editor = {};
std::unique_ptr<SRScene> g_Scene = {};
std::unique_ptr<SRCamera> g_Camera = {};

SRModel g_TestModel = {};
SRBuffer g_PerFrameBuffers[SRGraphicsDevice::FRAMES_IN_FLIGHT] = {};
PerFrameData g_PerFrameData = {};
SRSwapchain g_Swapchain = {};
SRSampler g_LinearSampler = {};

void init_console();
void init_window();
void init_graphics();
void init_resources();
void init_scene();
void init_rendergraph();
void update(const SRFrameInfo& frameInfo);
void render(const SRFrameInfo& frameInfo);

int APIENTRY wWinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR lpCmdLine,
	_In_ int nCmdShow
) {
	UNREFERENCED_PARAMETER(hInstance);
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);
	UNREFERENCED_PARAMETER(nCmdShow);

#ifdef _DEBUG
	init_console();
#endif

	init_window();
	init_graphics();
	init_resources();
	init_scene();
	init_rendergraph();
	g_GfxDevice->flush_initial_uploads(); // TEMPORARY but important for now

	SRFrameInfo frameInfo = {
		.camera = g_Camera.get(),
		.scene = g_Scene.get(),
		.dt = 0.0f,
		.width = WIDTH,
		.height = HEIGHT
	};

	SRInput::initialize(g_Window.get());
	SRTime::initialize();

	// Main loop
	bool firstFrame = true;
	while (g_Window->poll_events()) {
		SRTime::begin_frame();
		frameInfo.perFrameBuffer = &g_PerFrameBuffers[g_GfxDevice->get_frame_index()];
		frameInfo.dt = static_cast<float>(SRTime::get_delta_sec());
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
	if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
		AllocConsole();
	}

	FILE* fp;
	freopen_s(&fp, "CONOUT$", "w", stdout);
	freopen_s(&fp, "CONOUT$", "w", stderr);
	freopen_s(&fp, "CONIN$", "r", stdin);

	auto enable_vt = [](DWORD stdHandle) {
		HANDLE h = GetStdHandle(stdHandle);
		DWORD mode = 0;
		if (GetConsoleMode(h, &mode)) {
			mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
			mode |= DISABLE_NEWLINE_AUTO_RETURN;
			SetConsoleMode(h, mode);
		}
		};

	enable_vt(STD_OUTPUT_HANDLE);
	enable_vt(STD_ERROR_HANDLE);

	SetConsoleTitle(L"Stingray Console");
}

void init_window() {
	const char* windowTitle = (g_API == SRGraphicsAPI::Vulkan ? "Stingray (Vulkan)" : "Stingray (DX12)");
	g_Window = std::make_unique<SRWindow>(windowTitle, WIDTH, HEIGHT, SRWindowFlags_Centered | SRWindowFlags_SizeIsClientArea);
}

void init_graphics() {
	if constexpr (g_API == SRGraphicsAPI::Vulkan) {
		g_GfxDevice = std::make_unique<SRGraphicsDevice_Vulkan>(*g_Window);
	}
	else if constexpr (g_API == SRGraphicsAPI::DX12) {
		g_GfxDevice = std::make_unique<SRGraphicsDevice_DX12>(*g_Window);
	}
	g_ShaderCompiler = std::make_unique<SRShaderCompiler>(g_GfxDevice->get_shader_platform_info());

	const SRSwapchainInfo swapchainInfo = {
		.width = WIDTH,
		.height = HEIGHT,
		.numBuffers = 3,
		.format = SRFormat::RGBA8_UNORM,
		.vSync = true
	};
	g_GfxDevice->create_swapchain(swapchainInfo, g_Swapchain);
	g_Editor = std::make_unique<SREditor>(*g_Window, *g_GfxDevice, g_API);

	// Samplers
	const SRSamplerInfo linearSamplerInfo = {
		.filter = SRFilter::MinMagMipLinear,
		.addressU = SRTextureAddressMode::Wrap,
		.addressV = SRTextureAddressMode::Wrap,
		.addressW = SRTextureAddressMode::Wrap
	};
	g_GfxDevice->create_sampler(linearSamplerInfo, g_LinearSampler);
}

void init_resources() {
	const SRBufferInfo perFrameBufferInfo = {
		.size = sizeof(PerFrameData),
		.stride = sizeof(PerFrameData),
		.usage = SRUsage::Upload,
		.bindFlags = SRBindFlag::ConstantBuffer
	};

	for (uint32_t f = 0; f < SRGraphicsDevice::FRAMES_IN_FLIGHT; ++f) {
		g_GfxDevice->create_buffer(perFrameBufferInfo, g_PerFrameBuffers[f], &g_PerFrameData);
	}

	SRModelLoader::load_gltf(RES_DIR "Models/StanfordBunny/StanfordBunny.gltf", g_TestModel, *g_GfxDevice);
}

void init_scene() {
	g_Scene = std::make_unique<SRScene>(*g_GfxDevice, 65536);

	const SREntityID entity = g_Scene->add_entity();
	g_Scene->add_component<SRTransform>(entity, SRTransform{});
	g_Scene->add_component<SRRenderable>(entity, SRRenderable{ &g_TestModel });

	g_Camera = std::make_unique<SRCamera>(
		glm::vec3(0.0f, 0.0f, -2.0f),
		glm::angleAxis(glm::radians(0.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
		60.0f,
		g_Window->get_client_aspect_ratio(),
		0.01f,
		20.0f
	);
}

void init_rendergraph() {
	g_RenderGraph = std::make_unique<SRRenderGraph>();

	auto& depthPrepass = g_RenderGraph->add_render_pass("DepthPrepass", SRPassType::Graphics)
		.add_depth_output("Depth", WIDTH, HEIGHT, SRFormat::D32_FLOAT)
		.set_execute_callback(SRDepthPrepass::execute);
	SRDepthPrepass::build(depthPrepass, *g_GfxDevice, *g_ShaderCompiler);

	auto& gBufferPass = g_RenderGraph->add_render_pass("GBufferPass", SRPassType::Graphics)
		.add_depth_input("Depth")
		.add_color_output("GBufferAlbedo", WIDTH, HEIGHT, SRFormat::RGBA8_UNORM)
		.set_execute_callback(SRGBufferPass::execute);
	SRGBufferPass::build(gBufferPass, *g_GfxDevice, *g_ShaderCompiler);

	auto& compositionPass = g_RenderGraph->add_render_pass("CompositionPass", SRPassType::Graphics)
		.add_color_input("GBufferAlbedo", SRAccessFlag::Read)
		.set_execute_callback(SRCompositionPass::execute);
	SRCompositionPass::build(compositionPass, *g_GfxDevice, *g_ShaderCompiler);

	// TODO: Update render graph to respect mesh shading pipeline
	//auto& meshletPass = g_RenderGraph->add_render_pass("MeshletPass", SRPassType::Graphics)
	//	.set_execute_callback(SRMeshletGenerationpass::execute);
	//SRMeshletGenerationpass::build(meshletPass, *g_GfxDevice, *g_ShaderCompiler);
	//(void)meshletPass;

	auto& imguiPass = g_RenderGraph->add_render_pass("ImGuiPass", SRPassType::Graphics)
		.set_execute_callback([&](SRRenderPass& self, SRGraphicsDevice& gfxDevice, const SRCmdList& cmdList, const SRFrameInfo& frameInfo) {
			g_Editor->update(*g_RenderGraph);
			g_Editor->render(cmdList);
		});

	g_RenderGraph->build(*g_GfxDevice);
}

void update(const SRFrameInfo& frameInfo) {
	SRInput::update();
	SRMouseState mouse = SRInput::get_mouse_state();
	//SRLOG_TRACE("Mouse Delta: %d, %d", mouse.dx, mouse.dy);

	const float cameraMoveSpeed = 0.9f;
	const float mouseSensitivity = 0.001f;
	const float dx = mouseSensitivity * mouse.dx;
	const float dy = mouseSensitivity * mouse.dy;

	if (mouse.buttonStates & SRMouseButton_Middle) {
		glm::quat orientation = g_Camera->get_orientation();
		orientation = orientation * glm::angleAxis(dy, glm::vec3(1.0f, 0.0f, 0.0f)); // pitch
		orientation = glm::angleAxis(dx, glm::vec3(0.0f, 1.0f, 0.0f)) * orientation; // yaw
		g_Camera->set_orientation(orientation);
	}

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
