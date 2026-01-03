#include "Core/Logger.h"
#include "Core/Window.h"
#include "Core/System/Time.h"
#include "Data/ArenaAllocator.h"
#include "Data/Camera.h"
#include "Data/ComponentTypes.h"
#include "Data/Model.h"
#include "Data/Scene.h"
#include "Graphics/FrameInfo.h"
#include "Graphics/GraphicsDevice.h"
#include "Graphics/RenderGraph.h"
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

struct alignas(256) PerFrameData {
	glm::mat4 view    = { 1.0f };
	glm::mat4 proj    = { 1.0f };
	glm::mat4 invView = { 1.0f };
	glm::mat4 invProj = { 1.0f };
};

// NOTE: Trick for making sure that the logger exists longer than all other objects
static auto& logger = SRLogger::get();
static constexpr int WIDTH = 1920;
static constexpr int HEIGHT = 1080;

SRArena* g_Arena;
SRWindow* g_Window;
SRRenderGraph* g_RenderGraph;
SRShaderCompiler* g_ShaderCompiler;
SREditor* g_Editor;
SRScene* g_Scene;
SRCamera* g_Camera;

SRRenderPass* depthPrepass;
SRRenderPass* compositionPass;
SRRenderPass* gBufferPass;
SRRenderPass* imguiPass;

SRGFXBackend g_GfxBackend = SRGFXBackend::Vulkan;
SRGFXDevice g_GfxDevice;

SRModel g_TestModel = {};
SRBuffer g_PerFrameBuffers[SR_GFX_FRAMES_IN_FLIGHT] = {};
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
	HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPWSTR lpCmdLine,
	int nCmdShow
) {
	#ifdef _DEBUG
		init_console();
	#endif

	g_Arena = SRArena_Create();

	init_window();
	init_graphics();
	init_resources();
	init_scene();
	init_rendergraph();
	SRGFX_FlushInitialUploads(&g_GfxDevice); // TEMPORARY but important for now

	SRFrameInfo frameInfo = {
		.camera = g_Camera,
		.scene = g_Scene,
		.dt = 0.0f,
		.width = WIDTH,
		.height = HEIGHT
	};

	SRInput::initialize(g_Window);
	SRTime::initialize();

	// Main loop
	bool firstFrame = true;
	while (g_Window->poll_events()) {
		SRTime::begin_frame();

		SRGFX_BeginFrame(&g_GfxDevice, &g_Swapchain);
		frameInfo.perFrameBuffer = &g_PerFrameBuffers[SRGFX_GetFrameIndex(&g_GfxDevice)];
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
	SRGFX_WaitForGPU(&g_GfxDevice);

	SRShaderCompiler_Destroy(g_ShaderCompiler);

	// TODO: Temporary destruction logic, we will get rid of this eventually
	for (u32 f = 0; f < SR_GFX_FRAMES_IN_FLIGHT; ++f) { 
		SRGFX_DestroyResource(&g_GfxDevice, &g_PerFrameBuffers[f]);
	}

	auto* depthPrepassData = depthPrepass->get_pass_data<DepthPrepassData>();
	auto* gBufferPassData = gBufferPass->get_pass_data<GBufferPassData>();
	auto* compositionPassData = compositionPass->get_pass_data<CompositionPassData>();
	
	SRGFX_DestroyPipeline(&g_GfxDevice, &depthPrepassData->pipeline);
	SRGFX_DestroyPipeline(&g_GfxDevice, &gBufferPassData->pipeline);
	SRGFX_DestroyPipeline(&g_GfxDevice, &compositionPassData->pipeline);
	SRGFX_DestroyResource(&g_GfxDevice, &g_TestModel.vertexBuffer);
	SRGFX_DestroyResource(&g_GfxDevice, &g_TestModel.indexBuffer);
	SRGFX_DestroyResource(&g_GfxDevice, &g_TestModel.meshletBuffer);
	SRGFX_DestroyResource(&g_GfxDevice, &g_TestModel.meshletVerticesBuffer);
	SRGFX_DestroyResource(&g_GfxDevice, &g_TestModel.meshletTrianglesBuffer);
	SRGFX_DestroySwapchain(&g_GfxDevice, &g_Swapchain);

	delete g_Camera;
	delete g_Scene;
	delete g_Editor;
	delete g_RenderGraph;
	delete g_Window;
	SRGFX_DestroyDevice(&g_GfxDevice);
	SRArena_Destroy(g_Arena);

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
	const char* windowTitle = (g_GfxBackend == SRGFXBackend::Vulkan ? "Stingray (Vulkan)" : "Stingray (DX12)");
	g_Window = new SRWindow(windowTitle, WIDTH, HEIGHT, SRWindowFlags_Centered | SRWindowFlags_SizeIsClientArea);
}

void init_graphics() {
	SRGFX_CreateDevice(g_Window, &g_GfxDevice, g_GfxBackend);
	g_ShaderCompiler = SRShaderCompiler_Create(g_Arena, g_GfxBackend);

	SRSwapchainInfo swapchainInfo = {
		.width = WIDTH,
		.height = HEIGHT,
		.numBuffers = 3,
		.format = SRFormat::RGBA8_UNORM,
		.vSync = false
	};
	SRGFX_CreateSwapchain(&g_GfxDevice, g_Window, &swapchainInfo, &g_Swapchain);
	g_Editor = new SREditor(*g_Window, g_GfxDevice, g_GfxBackend);

	// Samplers
	SRSamplerInfo linearSamplerInfo = {
		.filter = SRFilter::MinMagMipLinear,
		.addressU = SRTextureAddressMode::Wrap,
		.addressV = SRTextureAddressMode::Wrap,
		.addressW = SRTextureAddressMode::Wrap
	};
	SRGFX_CreateSampler(&g_GfxDevice, &linearSamplerInfo, &g_LinearSampler);
}

void init_resources() {
	SRBufferInfo perFrameBufferInfo = {
		.size = sizeof(PerFrameData),
		.stride = sizeof(PerFrameData),
		.usage = SRUsage::Upload,
		.bindFlags = SRBindFlag::ConstantBuffer
	};

	for (u32 f = 0; f < SR_GFX_FRAMES_IN_FLIGHT; ++f) {
		SRGFX_CreateBuffer(&g_GfxDevice, &perFrameBufferInfo, &g_PerFrameBuffers[f], &g_PerFrameData);
	}

	SRModelLoader::load_gltf(RES_DIR "Models/StanfordBunny/StanfordBunny.gltf", g_TestModel, g_GfxDevice);
}

void init_scene() {
	g_Scene = new SRScene(g_GfxDevice, 65536);

	SREntityID entity = g_Scene->add_entity();
	g_Scene->add_component<SRTransform>(entity, SRTransform{});
	g_Scene->add_component<SRRenderable>(entity, SRRenderable{ &g_TestModel });

	g_Camera = new SRCamera(
		glm::vec3(0.0f, 0.1f, -0.3f),
		glm::angleAxis(glm::radians(0.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
		60.0f,
		g_Window->get_client_aspect_ratio(),
		0.01f,
		20.0f
	);
}

void init_rendergraph() {
	g_RenderGraph = new SRRenderGraph();

	depthPrepass = &g_RenderGraph->add_render_pass("DepthPrepass", SRPassType::Graphics)
		.add_depth_output("Depth", WIDTH, HEIGHT, SRFormat::D32_FLOAT)
		.set_execute_callback(SRDepthPrepass::execute);
	SRDepthPrepass::build(*depthPrepass, g_GfxDevice, *g_ShaderCompiler);

	gBufferPass = &g_RenderGraph->add_render_pass("GBufferPass", SRPassType::Graphics)
		.add_depth_input("Depth")
		.add_color_output("GBufferAlbedo", WIDTH, HEIGHT, SRFormat::RGBA8_UNORM)
		.set_execute_callback(SRGBufferPass::execute);
	SRGBufferPass::build(*gBufferPass, g_GfxDevice, *g_ShaderCompiler);

	compositionPass = &g_RenderGraph->add_render_pass("CompositionPass", SRPassType::Graphics)
		.add_color_input("GBufferAlbedo", SRAccessFlag::Read)
		.set_execute_callback(SRCompositionPass::execute);
	SRCompositionPass::build(*compositionPass, g_GfxDevice, *g_ShaderCompiler);

	// TODO: Update render graph to respect mesh shading pipeline
	//auto& meshletPass = g_RenderGraph->add_render_pass("MeshletPass", SRPassType::Graphics)
	//	.set_execute_callback(SRMeshletGenerationpass::execute);
	//SRMeshletGenerationpass::build(meshletPass, g_GfxDevice, *g_ShaderCompiler);

	imguiPass = &g_RenderGraph->add_render_pass("ImGuiPass", SRPassType::Graphics)
		.set_execute_callback([&](SRRenderPass& self, SRGFXDevice& gfxDevice, const SRCmdList& cmdList, const SRFrameInfo& frameInfo) {
			g_Editor->update(*g_RenderGraph);
			g_Editor->render(cmdList);
		});

	g_RenderGraph->build(g_GfxDevice);
}

void update(const SRFrameInfo& frameInfo) {
	SRInput::update();
	SRMouseState mouse = SRInput::get_mouse_state();
	//SRLOG_TRACE("Mouse Delta: %d, %d", mouse.dx, mouse.dy);

	f32 cameraMoveSpeed = 0.9f;
	f32 mouseSensitivity = 0.001f;
	f32 dx = mouseSensitivity * mouse.dx;
	f32 dy = mouseSensitivity * mouse.dy;

	if (mouse.buttonStates & SRMouseButton_Middle) {
		glm::quat orientation = g_Camera->get_orientation();
		orientation = orientation * glm::angleAxis(dy, glm::vec3(1.0f, 0.0f, 0.0f)); // pitch
		orientation = glm::angleAxis(dx, glm::vec3(0.0f, 1.0f, 0.0f)) * orientation; // yaw
		g_Camera->set_orientation(orientation);
	}

	glm::vec3 camRight = g_Camera->get_right();
	glm::vec3 camUp = g_Camera->get_up();
	glm::vec3 camForward = g_Camera->get_forward();
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

	std::memcpy(g_PerFrameBuffers[SRGFX_GetFrameIndex(&g_GfxDevice)].mappedData, &g_PerFrameData, sizeof(g_PerFrameData));
}

void render(const SRFrameInfo& frameInfo) {
	SRCmdList cmdList = SRGFX_BeginCommandList(&g_GfxDevice, SRQueue_Universal);
	g_RenderGraph->execute(g_GfxDevice, g_Swapchain, cmdList, frameInfo);
	SRGFX_SubmitCommandLists(&g_GfxDevice, &g_Swapchain);
}
