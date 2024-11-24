#include <GLFW/glfw3.h>

#include "platform.h"
#include "frame_info.h"
#include "data/camera.h"
#include "data/scene.h"
#include "ecs/ecs.h"
#include "input/input.h"

#include "graphics/render_graph.h"
#include "graphics/renderpasses/fullscreen_tri_pass.h"
#include "graphics/renderpasses/ray_tracing_pass.h"
#include "graphics/renderpasses/ui_pass.h"
#include "graphics/vulkan/gfx_device_vulkan.h"

#include "managers/asset_manager.h"

#include <glm/glm.hpp>

#include <chrono>
#include <cstdint>
#include <format>
#include <iostream>
#include <memory>
#include <vector>

struct PerFrameData {
	glm::mat4 projectionMatrix = { 1.0f };
	glm::mat4 viewMatrix = { 1.0f };
	glm::mat4 invViewProjection = { 1.0f };
	glm::vec3 cameraPosition = { 0.0f, 0.0f, 0.0f };
	uint32_t pad1 = 0;
};

GLOBAL_VARIABLE GraphicsAPI g_API = GraphicsAPI::VULKAN;
GLOBAL_VARIABLE int g_FrameWidth = 1920;
GLOBAL_VARIABLE int g_FrameHeight = 1080;
GLOBAL_VARIABLE GLFWwindow* g_Window = nullptr;
GLOBAL_VARIABLE UIEvent g_MouseEvent = UIEvent(UIEventType::None);
GLOBAL_VARIABLE UIEvent g_KeyboardEvent = UIEvent(UIEventType::None);

GLOBAL_VARIABLE std::unique_ptr<GFXDevice> g_GfxDevice = {};
GLOBAL_VARIABLE std::unique_ptr<FullscreenTriPass> g_FullscreenTriPass = {};
GLOBAL_VARIABLE std::unique_ptr<RayTracingPass> g_RayTracingPass = {};
GLOBAL_VARIABLE std::unique_ptr<UIPass> g_UIPass = {};
GLOBAL_VARIABLE std::unique_ptr<RenderGraph> g_RenderGraph = {};
GLOBAL_VARIABLE SwapChain g_SwapChain = {};
GLOBAL_VARIABLE Sampler g_DefaultSampler = {};

GLOBAL_VARIABLE Buffer g_PerFrameDataBuffers[GFXDevice::FRAMES_IN_FLIGHT] = {};
GLOBAL_VARIABLE PerFrameData g_PerFrameData = {};

GLOBAL_VARIABLE std::unique_ptr<Camera> g_Camera = {};
GLOBAL_VARIABLE FrameInfo g_FrameInfo = {};
GLOBAL_VARIABLE uint64_t g_LastFrameCount = 0;
GLOBAL_VARIABLE uint64_t g_CurrentFPS = 0;
GLOBAL_VARIABLE std::chrono::high_resolution_clock::time_point g_FPSStartTime = {};
GLOBAL_VARIABLE std::unique_ptr<Scene> g_Scene = {};

// Resources
GLOBAL_VARIABLE Texture g_DefaultAlbedoMap = {};
GLOBAL_VARIABLE Texture g_DefaultNormalMap = {};
GLOBAL_VARIABLE Asset g_CubeModel = {};
GLOBAL_VARIABLE Asset g_TestTexture = {};
GLOBAL_VARIABLE Asset g_SponzaModel = {};
GLOBAL_VARIABLE Asset g_PlaneModel = {};
GLOBAL_VARIABLE std::unique_ptr<Model> g_Sphere = {};

// Callbacks
INTERNAL void resize_callback(GLFWwindow* window, int width, int height) {
	g_FrameWidth = width;
	g_FrameHeight = height;
}

INTERNAL void mouse_position_callback(GLFWwindow* window, double xpos, double ypos) {
	input::process_mouse_position(xpos, ypos);

	if (g_UIPass != nullptr) {
		g_MouseEvent.set_type(UIEventType::MouseMove);

		MouseEventData& mouse = g_MouseEvent.get_mouse_data();
		mouse.position.x = xpos;
		mouse.position.y = ypos;

		g_UIPass->process_event(g_MouseEvent);
	}
}

INTERNAL void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
	input::process_mouse_event(button, action, mods);

	if (g_UIPass != nullptr) {
		MouseEventData& mouse = g_MouseEvent.get_mouse_data();
		g_MouseEvent.set_type(action == GLFW_PRESS ? UIEventType::MouseDown : UIEventType::MouseUp);

		if (button == GLFW_MOUSE_BUTTON_1) {
			mouse.downButtons.left = action == GLFW_PRESS;
		}
		else if (button == GLFW_MOUSE_BUTTON_2) {
			mouse.downButtons.right = action == GLFW_PRESS;
		}
		else if (button == GLFW_MOUSE_BUTTON_3) {
			mouse.downButtons.middle = action == GLFW_PRESS;
		}

		g_UIPass->process_event(g_MouseEvent);
	}
}

INTERNAL void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
		glfwSetWindowShouldClose(g_Window, true);
	}

	input::process_key_event(key, scancode, action, mods);
	
	if (g_UIPass != nullptr) {
		if (action == GLFW_PRESS) {
			g_KeyboardEvent.set_type(UIEventType::KeyboardDown);
		}
		else {
			g_KeyboardEvent.set_type(UIEventType::KeyboardUp);
		}

		KeyboardEventData& keyboard = g_KeyboardEvent.get_keyboard_data();
		keyboard.key = key;
		keyboard.action = action;
		keyboard.mods = mods;

		g_UIPass->process_event(g_KeyboardEvent);
	}
}

INTERNAL void key_char_callback(GLFWwindow* window, unsigned int codepoint) {
	if (g_UIPass != nullptr) {
		UIEvent keyCharEvent(UIEventType::KeyboardChar);
		KeyboardCharData& keyCharData = keyCharEvent.get_keyboard_char_data();
		keyCharData.codePoint = codepoint;

		g_UIPass->process_event(keyCharEvent);
	}
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

	const std::string title = "Stingray " + apiString;

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
		.filter = Filter::ANISOTROPIC,
		.maxAnisotropy = 16
	};
	g_GfxDevice->create_sampler(defaultSamplerInfo, g_DefaultSampler);
}

INTERNAL void init_render_graph() {
	// Initialize passes
	g_RayTracingPass = std::make_unique<RayTracingPass>(*g_GfxDevice, *g_Scene);
	g_FullscreenTriPass = std::make_unique<FullscreenTriPass>(*g_GfxDevice);
	g_UIPass = std::make_unique<UIPass>(*g_GfxDevice, g_Window);

	// Create render graph
	const uint32_t uWidth = static_cast<uint32_t>(g_FrameWidth);
	const uint32_t uHeight = static_cast<uint32_t>(g_FrameHeight);

	g_RenderGraph = std::make_unique<RenderGraph>(*g_GfxDevice);

	auto rtPass = g_RenderGraph->add_pass("RayTracingPass");
	rtPass->add_output_attachment("RTOutput", AttachmentInfo{ uWidth, uHeight, AttachmentType::RW_TEXTURE, Format::R8G8B8A8_UNORM });
	rtPass->set_execute_callback([&](PassExecuteInfo& executeInfo) {
		g_RayTracingPass->execute(executeInfo, *g_Scene);
	});

	auto fullscreenTriPass = g_RenderGraph->add_pass("FullScreenTriPass");
	fullscreenTriPass->add_input_attachment("RTOutput");
	fullscreenTriPass->set_execute_callback([&](PassExecuteInfo& executeInfo) {
		g_FullscreenTriPass->execute(executeInfo);
	});

	auto uiPass = g_RenderGraph->add_pass("UIPass");
	uiPass->set_execute_callback([&](PassExecuteInfo& executeInfo) {
		g_UIPass->execute(executeInfo);
	});

	g_RenderGraph->build();
}

INTERNAL void init_objects() {
	g_Scene = std::make_unique<Scene>(*g_GfxDevice);

	assetmanager::initialize(*g_GfxDevice);
	assetmanager::load_from_file(g_CubeModel, "models/cube/cube.gltf");
	assetmanager::load_from_file(g_PlaneModel, "models/thin_plane/thin_plane.gltf");
	g_Sphere = assetmanager::create_sphere(1.0f, 32, 64);

	g_Camera = std::make_unique<Camera>(
		glm::vec3(0, 3.0f, -4.0f),
		glm::angleAxis(glm::radians(0.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
		glm::radians(60.0f),
		(float)g_FrameWidth / g_FrameHeight,
		0.1f,
		100.0f
	);

	// Entities
	ecs::initialize();

	const entity_id sphere = g_Scene->add_entity("Sphere");
	ecs::add_component<Renderable>(sphere, Renderable{ g_Sphere.get() });
	ecs::get_component<Transform>(sphere)->position = { -0.1f, 1.5f, 0.0f };
	ecs::get_component<Material>(sphere)->color = { 0.0f, 0.0f, 1.0f };

	entity_id plane = g_Scene->add_entity("Plane");
	ecs::add_component<Renderable>(plane, Renderable{ g_PlaneModel.get_model() });
	ecs::get_component<Transform>(plane)->position = { 0.0f, 0.0f, 0.0f };
	ecs::get_component<Transform>(plane)->scale = glm::vec3(10.0f);
	ecs::get_component<Material>(plane)->color = { 1.0f, 1.0f, 1.0f };

	entity_id backWall = g_Scene->add_entity("Back Wall");
	ecs::add_component<Renderable>(backWall, Renderable{ g_PlaneModel.get_model() });
	ecs::get_component<Transform>(backWall)->position = { 0.0f, 5.0f, 5.0f };
	ecs::get_component<Transform>(backWall)->orientation = glm::angleAxis(-glm::half_pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f));
	ecs::get_component<Transform>(backWall)->scale = glm::vec3(10.0f);
	ecs::get_component<Material>(backWall)->color = { 1.0f, 1.0f, 1.0f };

	entity_id leftWall = g_Scene->add_entity("Left Wall");
	ecs::add_component<Renderable>(leftWall, Renderable{ g_PlaneModel.get_model() });
	ecs::get_component<Transform>(leftWall)->position = { -5.0f, 5.0f, 0.0f };
	ecs::get_component<Transform>(leftWall)->orientation = glm::angleAxis(-glm::half_pi<float>(), glm::vec3(0.0f, 0.0f, 1.0f));
	ecs::get_component<Transform>(leftWall)->scale = glm::vec3(10.0f);
	ecs::get_component<Material>(leftWall)->color = { 1.0f, 0.0f, 0.0f };

	entity_id rightWall = g_Scene->add_entity("Right Wall");
	ecs::add_component<Renderable>(rightWall, Renderable{ g_PlaneModel.get_model() });
	ecs::get_component<Transform>(rightWall)->position = { 5.0f, 5.0f, 0.0f };
	ecs::get_component<Transform>(rightWall)->orientation = glm::angleAxis(glm::half_pi<float>(), glm::vec3(0.0f, 0.0f, 1.0f));
	ecs::get_component<Transform>(rightWall)->scale = glm::vec3(10.0f);
	ecs::get_component<Material>(rightWall)->color = { 0.0f, 1.0f, 0.0f };
}

INTERNAL void on_update(FrameInfo& frameInfo) {
	// UI
	g_UIPass->widget_slider_float("Sun Direction X", &g_Scene->m_SunDirection.x, -1.0f, 1.0f);
	g_UIPass->widget_slider_float("Sun Direction Y", &g_Scene->m_SunDirection.y, -1.0f, 1.0f);
	g_UIPass->widget_slider_float("Sun Direction Z", &g_Scene->m_SunDirection.z, -1.0f, 1.0f);

	// Input
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
	g_PerFrameData.invViewProjection = camera.getInvViewProjMatrix();
	g_PerFrameData.cameraPosition = camera.getPosition();

	std::memcpy(g_PerFrameDataBuffers[g_GfxDevice->get_frame_index()].mappedData, &g_PerFrameData, sizeof(g_PerFrameData));
}

int main() {
	if (!init_glfw(&g_Window)) { return -1; }

	// Callbacks
	glfwSetFramebufferSizeCallback(g_Window, resize_callback);
	glfwSetCursorPosCallback(g_Window, mouse_position_callback);
	glfwSetMouseButtonCallback(g_Window, mouse_button_callback);
	glfwSetKeyCallback(g_Window, key_callback);
	glfwSetCharCallback(g_Window, key_char_callback);

	init_gfx();
	init_objects();
	init_render_graph();

	g_FrameInfo.camera = g_Camera.get();

	while (!glfwWindowShouldClose(g_Window)) {
		glfwPollEvents();

		static auto lastTime = std::chrono::high_resolution_clock::now();
		const auto currentTime = std::chrono::high_resolution_clock::now();
		const float dt = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - lastTime).count();
		lastTime = currentTime;

		// Update FPS counter
		if (std::chrono::duration<float, std::chrono::seconds::period>(
			currentTime - g_FPSStartTime).count() >= 1.0f) {
			g_FPSStartTime = currentTime;
			g_CurrentFPS = g_GfxDevice->get_frame_count() - g_LastFrameCount;
			g_LastFrameCount = g_GfxDevice->get_frame_count();
		}
		
		// Update
		g_FrameInfo.dt = dt;
		// TODO: Make dynamic
		g_FrameInfo.width = g_FrameWidth;
		g_FrameInfo.height = g_FrameHeight;
		on_update(g_FrameInfo);

		g_UIPass->widget_text(std::format("FPS: {}", g_CurrentFPS));

		// Rendering
		CommandList cmdList = g_GfxDevice->begin_command_list(QueueType::DIRECT);

		static bool builtASes = false;

		if (!builtASes) {
			g_RayTracingPass->build_acceleration_structures(cmdList);
			builtASes = true;
		}

		g_RenderGraph->execute(g_SwapChain, cmdList, g_FrameInfo);
		g_GfxDevice->submit_command_lists(g_SwapChain);
	}

	g_GfxDevice->wait_for_gpu();

	// Shutdown
	ecs::destroy();
	assetmanager::destroy();
	glfwDestroyWindow(g_Window);
	glfwTerminate();

	return 0;
}
