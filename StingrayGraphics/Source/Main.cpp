#include <Windows.h>
#include "Core/Logger.hpp"
#include "Core/Window.hpp"
#include "Core/System/Time.hpp"
#include "Data/Camera.hpp"
#include "Data/Model.hpp"
#include "Graphics/GraphicsDevice.hpp"
#include "Graphics/DX12/GraphicsDevice_DX12.hpp"
#include "Graphics/Vulkan/GraphicsDevice_Vulkan.hpp"
#include "Graphics/ShaderCompiler.hpp"
#include "Input/Input.hpp"

#include <glm/glm.hpp>
#include <cassert>
#include <iostream>
#include <memory>
#include <vector>

struct FrameInfo {
	SRCamera* camera;
	float dt;
};

struct alignas(256) PerFrameData {
	glm::mat4 view = { 1.0f };
	glm::mat4 proj = { 1.0f };
	glm::mat4 invView = { 1.0f };
	glm::mat4 invProj = { 1.0f };
};

static std::vector<glm::vec3> VERTICES = {
	{ -0.5f, -0.5f, 0.0f },
	{  0.5f, -0.5f, 0.0f },
	{  0.0f,  0.5f, 0.0f },
};

// NOTE: Trick for making sure that the logger exists longer than all other objects
static auto& logger = SRLogger::get();
static constexpr SRGraphicsAPI g_API = SRGraphicsAPI::DX12;
static constexpr int WIDTH = 1920;
static constexpr int HEIGHT = 1080;

std::unique_ptr<SRWindow> g_Window = {};
std::unique_ptr<SRGraphicsDevice> g_GfxDevice = {};
std::unique_ptr<SRShaderCompiler> g_ShaderCompiler = {};
std::unique_ptr<SRCamera> g_Camera = {};

SRBuffer g_VertexBuffer = {};
SRBuffer g_PerFrameBuffers[SRGraphicsDevice::FRAMES_IN_FLIGHT] = {};
PerFrameData g_PerFrameData = {};
SRShader g_VertexShader = {};
SRShader g_PixelShader  = {};
SRPipeline g_Pipeline   = {};
SRSwapchain g_Swapchain = {};
SRModel g_TestModel = {};

void init_console();
void init_resources();
void update(const FrameInfo& frameInfo);
void render();

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
	g_ShaderCompiler->compile_from_file(RES_DIR "Shaders/Testing.slang", { SRShaderStage::VERTEX, "vertexMain" }, g_VertexShader);
	g_ShaderCompiler->compile_from_file(RES_DIR "Shaders/Testing.slang", { SRShaderStage::PIXEL, "pixelMain" }, g_PixelShader);

	init_resources();

	g_Camera = std::make_unique<SRCamera>(
		glm::vec3(0.0f, 0.0f, -2.0f),
		glm::angleAxis(glm::radians(0.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
		60.0f,
		g_Window->get_client_aspect_ratio(),
		0.1f,
		20.0f
	);

	const SRPipelineInfo pipelineInfo = {
		.vertexShader = &g_VertexShader,
		.pixelShader = &g_PixelShader,
		.inputLayout = {
			.elements = {
				{ "POSITION", SRFormat::RGB32_FLOAT },
				{ "TEXCOORD", SRFormat::RG32_FLOAT }
			}
		},
		.numRenderTargets = 1,
		.renderTargetFormats = { SRFormat::BGRA8_UNORM },
	};
	g_GfxDevice->create_pipeline(pipelineInfo, g_Pipeline);

	const SRSwapchainInfo swapchainInfo = {
		.width = WIDTH,
		.height = HEIGHT,
		.numBuffers = 3,
		.format = SRFormat::BGRA8_UNORM,
		.vSync = true
	};
	g_GfxDevice->create_swapchain(swapchainInfo, g_Swapchain);

	bool firstFrame = true;
	FrameInfo frameInfo = {
		.camera = g_Camera.get(),
		.dt = 0.0f
	};

	SRTime::initialize();
	while (g_Window->poll_events()) {
		SRTime::begin_frame();
		frameInfo.dt = SRTime::get_delta_sec();

		update(frameInfo);
		render();

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

	SRModelLoader::load_gltf(RES_DIR "Models/Cube/cube.gltf", g_TestModel, *g_GfxDevice);

	g_GfxDevice->flush_initial_uploads();
}

void update(const FrameInfo& frameInfo) {
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

void render() {
	const SRCmdList cmdList = g_GfxDevice->begin_command_list(SRQueue_Universal);
	{
		g_GfxDevice->begin_render_pass(g_Swapchain, cmdList);
		{
			const SRViewport viewport = {
				.width = static_cast<float>(WIDTH),
				.height = static_cast<float>(HEIGHT),
			};
			g_GfxDevice->bind_viewport(viewport, cmdList);
			g_GfxDevice->bind_pipeline(g_Pipeline, cmdList);
			g_GfxDevice->bind_root_constant_buffer(g_PerFrameBuffers[g_GfxDevice->get_frame_index()], cmdList);
			g_GfxDevice->bind_vertex_buffer(g_TestModel.vertexBuffer, cmdList);
			g_GfxDevice->bind_index_buffer(g_TestModel.indexBuffer, cmdList);

			for (const auto& mesh : g_TestModel.meshes) {
				for (uint32_t i = mesh.basePrimitive; i < mesh.numPrimitives; ++i) {
					const SRMeshPrimitive& primitive = g_TestModel.primitives[i];

					g_GfxDevice->draw_indexed(primitive.numIndices, primitive.baseIndex, primitive.baseVertex, cmdList);
				}
			}
		}
		g_GfxDevice->end_render_pass(g_Swapchain, cmdList);
	}
	g_GfxDevice->submit_command_lists(g_Swapchain);
}
