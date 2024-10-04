#include <GLFW/glfw3.h>

#include "platform.h"
#include "frame_info.h"
#include "data/camera.h"
#include "ecs/ecs.h"
#include "input/input.h"
#include "graphics/render_graph.h"
#include "graphics/renderpasses/gbuffer_pass.h"
#include "graphics/renderpasses/lighting_pass.h"
#include "graphics/renderpasses/ui_pass.h"

#include "graphics/vulkan/gfx_device_vulkan.h"
#include "managers/asset_manager.h"

#include <glm/glm.hpp>

#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>

struct PerFrameData {
	glm::mat4 projectionMatrix = { 1.0f };
	glm::mat4 viewMatrix = { 1.0f };
};

struct Vertex {
	glm::vec3 position = {};
	glm::vec3 color = {};
};

GLOBAL_VARIABLE GraphicsAPI g_API = GraphicsAPI::VULKAN;
GLOBAL_VARIABLE int g_FrameWidth = 1920;
GLOBAL_VARIABLE int g_FrameHeight = 1080;
GLOBAL_VARIABLE GLFWwindow* g_Window = nullptr;

GLOBAL_VARIABLE std::unique_ptr<GFXDevice> g_GfxDevice = {};
GLOBAL_VARIABLE std::unique_ptr<GBufferPass> g_GBufferPass = {};
GLOBAL_VARIABLE std::unique_ptr<LightingPass> g_LightingPass = {};
GLOBAL_VARIABLE std::unique_ptr<UIPass> g_UIPass = {};
GLOBAL_VARIABLE std::unique_ptr<RenderGraph> g_RenderGraph = {};
GLOBAL_VARIABLE SwapChain g_SwapChain = {};
GLOBAL_VARIABLE Sampler g_DefaultSampler = {};

GLOBAL_VARIABLE Buffer g_PerFrameDataBuffers[GFXDevice::FRAMES_IN_FLIGHT] = {};
GLOBAL_VARIABLE PerFrameData g_PerFrameData = {};

GLOBAL_VARIABLE std::unique_ptr<Camera> g_Camera = {};
GLOBAL_VARIABLE FrameInfo g_FrameInfo = {};
GLOBAL_VARIABLE std::vector<entity_id> g_Entities = {};

// Resources
GLOBAL_VARIABLE Texture g_DefaultAlbedoMap = {};
GLOBAL_VARIABLE Texture g_DefaultNormalMap = {};
GLOBAL_VARIABLE Asset g_CubeModel = {};
GLOBAL_VARIABLE Asset g_TestTexture = {};
GLOBAL_VARIABLE Asset g_EarthTexture = {};
GLOBAL_VARIABLE std::unique_ptr<Model> g_Sphere = {};
GLOBAL_VARIABLE std::unique_ptr<Model> g_Plane = {};

// Callbacks
INTERNAL void resize_callback(GLFWwindow* window, int width, int height) {
	g_FrameWidth = width;
	g_FrameHeight = height;
	//glViewport(0, 0, width, height);
}

INTERNAL void mouse_position_callback(GLFWwindow* window, double xpos, double ypos) {
	input::process_mouse_position(xpos, ypos);
}

INTERNAL void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
	input::process_mouse_event(button, action, mods);
}

INTERNAL void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
		glfwSetWindowShouldClose(g_Window, true);
	}

	input::process_key_event(key, scancode, action, mods);
}

INTERNAL int init_glfw(GLFWwindow** window) {
	// Initialize GLFW
	if (!glfwInit()) {
		std::cout << "Failed to initialize GLFW.\n";
		return 0;
	}
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	const std::string apiString = [=]() {
		switch (g_API) {
		case GraphicsAPI::VULKAN:
			return "(Vulkan)";
		default:
			return "";
		}
	}();

	const std::string title = "gl-testbench " + apiString;

	*window = glfwCreateWindow(
		g_FrameWidth,
		g_FrameHeight,
		title.c_str(),
		nullptr,
		nullptr
	);

	if (*window == nullptr) {
		std::cout << "Failed to create GLFW window\n";
		glfwTerminate();
		return 0;
	}

	// Center window
	GLFWmonitor* monitor = glfwGetPrimaryMonitor();
	const GLFWvidmode* mode = glfwGetVideoMode(monitor);

	int screenSpaceWidth;
	int screenSpaceHeight;
	glfwGetWindowSize(*window, &screenSpaceWidth, &screenSpaceHeight);
	glfwSetWindowPos(
		*window,
		mode->width / 2 - screenSpaceWidth / 2,
		mode->height / 2 - screenSpaceHeight / 2
	);

	glfwMakeContextCurrent(*window);

	return 1;
}

INTERNAL void init_gfx() {
	switch (g_API) {
	case GraphicsAPI::VULKAN:
		{
			g_GfxDevice = std::make_unique<GFXDevice_Vulkan>(g_Window);
		}
		break;
	}

	const SwapChainInfo swapChainInfo = {
		.width = static_cast<uint32_t>(g_FrameWidth),
		.height = static_cast<uint32_t>(g_FrameHeight),
		.bufferCount = 3,
		.format = Format::R8G8B8A8_UNORM,
		.vsync = true
	};
	g_GfxDevice->create_swapchain(swapChainInfo, g_SwapChain);

	const BufferInfo perFrameDataBufferInfo = {
		.size = sizeof(PerFrameData),
		.stride = sizeof(PerFrameData),
		.usage = Usage::UPLOAD,
		.bindFlags = BindFlag::UNIFORM_BUFFER,
		.persistentMap = true
	};

	for (uint32_t i = 0; i < GFXDevice::FRAMES_IN_FLIGHT; ++i) {
		g_GfxDevice->create_buffer(perFrameDataBufferInfo, g_PerFrameDataBuffers[i], &g_PerFrameData);
	}

	// Default textures
	const TextureInfo textureInfo1x1 = {
		.width = 1,
		.height = 1,
		.format = Format::R8G8B8A8_UNORM,
		.bindFlags = BindFlag::SHADER_RESOURCE
	};

	const uint32_t defaultAlbedoMapData = 0xffffffff;
	const uint32_t defaultNormalMapData = 0xffff8080; // tangent space default normal
	const SubresourceData defaultAlbedoMapSubresource = { &defaultAlbedoMapData, sizeof(uint32_t) };
	const SubresourceData defaultNormalMapSubresource = { &defaultNormalMapData, sizeof(uint32_t) };

	g_GfxDevice->create_texture(textureInfo1x1, g_DefaultAlbedoMap, &defaultAlbedoMapSubresource);
	g_GfxDevice->create_texture(textureInfo1x1, g_DefaultNormalMap, &defaultNormalMapSubresource);

	// Default samplers
	const SamplerInfo defaultSamplerInfo = {

	};
	g_GfxDevice->create_sampler(defaultSamplerInfo, g_DefaultSampler);

	// Render graph
	const uint32_t uWidth = static_cast<uint32_t>(g_FrameWidth);
	const uint32_t uHeight = static_cast<uint32_t>(g_FrameHeight);

	g_GBufferPass = std::make_unique<GBufferPass>(*g_GfxDevice);
	g_LightingPass = std::make_unique<LightingPass>(*g_GfxDevice);
	g_UIPass = std::make_unique<UIPass>(*g_GfxDevice);

	g_RenderGraph = std::make_unique<RenderGraph>(*g_GfxDevice);
	auto gBufferPass = g_RenderGraph->add_pass("GBufferPass");
	gBufferPass->add_output_attachment("Position", AttachmentInfo{ uWidth, uHeight, AttachmentType::RENDER_TARGET, Format::R32G32B32A32_FLOAT });
	gBufferPass->add_output_attachment("Albedo", AttachmentInfo{ uWidth, uHeight, AttachmentType::RENDER_TARGET, Format::R8G8B8A8_UNORM });
	gBufferPass->add_output_attachment("Normal", AttachmentInfo{ uWidth, uHeight, AttachmentType::RENDER_TARGET, Format::R16G16B16A16_FLOAT });
	gBufferPass->add_output_attachment("Depth", AttachmentInfo{ uWidth, uHeight, AttachmentType::DEPTH_STENCIL, Format::D32_FLOAT });
	gBufferPass->set_execute_callback([&](PassExecuteInfo& executeInfo) {
		g_GBufferPass->execute(executeInfo, g_Entities);
	});

	auto lightingPass = g_RenderGraph->add_pass("LightingPass");
	lightingPass->add_input_attachment("Position");
	lightingPass->add_input_attachment("Albedo");
	lightingPass->add_input_attachment("Normal");
	lightingPass->set_execute_callback([&](PassExecuteInfo& executeInfo) {
		g_LightingPass->execute(executeInfo);
	});

	auto uiPass = g_RenderGraph->add_pass("UIPass");
	uiPass->set_execute_callback([&](PassExecuteInfo& executeInfo) {
		g_UIPass->execute(executeInfo);
	});

	g_RenderGraph->build();
}

INTERNAL void init_objects() {
	assetmanager::initialize(*g_GfxDevice);
	assetmanager::load_from_file(g_CubeModel, "resources/cube.gltf");
	assetmanager::load_from_file(g_EarthTexture, "resources/textures/earth.jpg");
	uint32_t t = g_GfxDevice->get_descriptor_index(*g_EarthTexture.get_texture());

	g_Sphere = assetmanager::create_sphere(1.0f, 32, 64);
	g_Plane = assetmanager::create_plane(5.0f, 5.0f);

	g_Camera = std::make_unique<Camera>(
		glm::vec3(0, 0, -4.0f),
		glm::angleAxis(glm::radians(0.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
		glm::radians(60.0f),
		(float)g_FrameWidth / g_FrameHeight,
		0.1f,
		100.0f
	);

	// Entities
	ecs::initialize();

	const entity_id entity = ecs::create_entity();
	ecs::add_component(entity, Renderable{ g_Sphere.get() });
	ecs::get_component_transform(entity)->position = { -0.1f, 0.5f, 0.0f };

	entity_id plane = ecs::create_entity();
	ecs::add_component(plane, Renderable{ g_Plane.get() });

	g_Entities.push_back(entity);
	g_Entities.push_back(plane);
}

INTERNAL void on_update(FrameInfo& frameInfo) {
	input::update();
	input::MouseState mouse = {};
	input::get_mouse_state(mouse);

	Camera& camera = *frameInfo.camera;
	const float cameraMoveSpeed = 5.0f;
	const float mouseSensitivity = 0.001f;

	if (mouse.buttons[2]) {
		glm::quat newOrientation = camera.getOrientation();

		if (mouse.deltaY != 0.0f) {
			newOrientation = newOrientation * glm::angleAxis(mouse.deltaY * mouseSensitivity, glm::vec3(1.0f, 0.0f, 0.0f)); // pitch
		}

		if (mouse.deltaX != 0.0f) {
			newOrientation = glm::angleAxis(mouse.deltaX * mouseSensitivity, glm::vec3(0.0f, 1.0f, 0.0f)) * newOrientation; // yaw
		}

		camera.setOrientation(newOrientation);
	}

	const glm::vec3 qRight = camera.getRight();
	const glm::vec3 qUp = camera.getUp();
	const glm::vec3 qForward = camera.getForward();

	glm::vec3 newPosition = camera.getPosition();

	if (input::is_key_down(GLFW_KEY_W)) {
		newPosition += qForward * cameraMoveSpeed * frameInfo.dt;
	}
	if (input::is_key_down(GLFW_KEY_A)) {
		newPosition -= qRight * cameraMoveSpeed * frameInfo.dt;
	}
	if (input::is_key_down(GLFW_KEY_S)) {
		newPosition -= qForward * cameraMoveSpeed * frameInfo.dt;
	}
	if (input::is_key_down(GLFW_KEY_D)) {
		newPosition += qRight * cameraMoveSpeed * frameInfo.dt;
	}

	if (input::is_key_down(GLFW_KEY_SPACE)) {
		newPosition.y += cameraMoveSpeed * frameInfo.dt;
	}
	if (input::is_key_down(GLFW_KEY_LEFT_CONTROL)) {
		newPosition.y -= cameraMoveSpeed * frameInfo.dt;
	}

	camera.setPosition(newPosition);

	const float aspectRatio = static_cast<float>(g_FrameWidth) / g_FrameHeight;
	camera.setAspectRatio(aspectRatio);

	camera.update();

	g_PerFrameData.projectionMatrix = camera.getProjMatrix();
	g_PerFrameData.viewMatrix = camera.getViewMatrix();
	g_GfxDevice->update_buffer(g_PerFrameDataBuffers[g_GfxDevice->get_frame_index()], &g_PerFrameData);

	std::memcpy(g_PerFrameDataBuffers[g_GfxDevice->get_frame_index()].mappedData, &g_PerFrameData, sizeof(g_PerFrameData));
}

int main() {
	if (!init_glfw(&g_Window)) { return -1; }

	// Callbacks
	glfwSetFramebufferSizeCallback(g_Window, resize_callback);
	glfwSetCursorPosCallback(g_Window, mouse_position_callback);
	glfwSetMouseButtonCallback(g_Window, mouse_button_callback);
	glfwSetKeyCallback(g_Window, key_callback);

	init_gfx();
	init_objects();
	g_FrameInfo.camera = g_Camera.get();

	float deltaTime = 0.0f;
	float lastFrame = 0.0f;

	while (!glfwWindowShouldClose(g_Window)) {
		const float currentFrame = glfwGetTime();
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;
		
		// Update
		g_FrameInfo.dt = deltaTime;
		// TODO: Make dynamic
		g_FrameInfo.width = g_FrameWidth;
		g_FrameInfo.height = g_FrameHeight;
		on_update(g_FrameInfo);

		// Rendering
		CommandList cmdList = g_GfxDevice->begin_command_list(QueueType::DIRECT);
		g_RenderGraph->execute(g_SwapChain, cmdList, g_FrameInfo);
		g_GfxDevice->submit_command_lists(g_SwapChain);

		glfwPollEvents();
	}

	g_GfxDevice->wait_for_gpu();

	// Shutdown
	ecs::destroy();
	glfwDestroyWindow(g_Window);
	glfwTerminate();
	return 0;
}
