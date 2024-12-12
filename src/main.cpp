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
GLOBAL_VARIABLE Scene* g_ActiveScene = nullptr;

// Resources
GLOBAL_VARIABLE Texture g_DefaultAlbedoMap = {};
GLOBAL_VARIABLE Texture g_DefaultNormalMap = {};
GLOBAL_VARIABLE Asset g_TestTexture = {};
GLOBAL_VARIABLE Asset g_PlaneModel = {};
GLOBAL_VARIABLE Asset g_LucyModel = {};
GLOBAL_VARIABLE Asset g_SponzaModel = {};
GLOBAL_VARIABLE std::unique_ptr<Model> g_FlatPlaneModel = {};
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
	g_RayTracingPass = std::make_unique<RayTracingPass>(*g_GfxDevice);
	g_FullscreenTriPass = std::make_unique<FullscreenTriPass>(*g_GfxDevice);
	g_UIPass = std::make_unique<UIPass>(*g_GfxDevice, g_Window);

	// Create render graph
	const uint32_t uWidth = static_cast<uint32_t>(g_FrameWidth);
	const uint32_t uHeight = static_cast<uint32_t>(g_FrameHeight);

	g_RenderGraph = std::make_unique<RenderGraph>(*g_GfxDevice);

	auto rtPass = g_RenderGraph->add_pass("RayTracingPass");
	rtPass->add_output_attachment("RTOutput", AttachmentInfo{ uWidth, uHeight, AttachmentType::RW_TEXTURE, Format::R8G8B8A8_UNORM });
	rtPass->add_output_attachment("RTAccumulation", AttachmentInfo{ uWidth, uHeight, AttachmentType::RW_TEXTURE, Format::R32G32B32A32_FLOAT });
	rtPass->set_execute_callback([&](PassExecuteInfo& executeInfo) {
		if (g_ActiveScene != nullptr) {
			g_RayTracingPass->execute(executeInfo, *g_ActiveScene);
		}
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

// ------------------------------- Create Scenes -------------------------------
INTERNAL void create_cornell_scene() {
	assetmanager::load_from_file(g_PlaneModel, "models/thin_plane/thin_plane.gltf");
	assetmanager::load_from_file(g_LucyModel, "models/lucy/lucy.gltf");
	g_Sphere = assetmanager::create_sphere(1.5f, 32, 64);
	assetmanager::load_from_file(g_TestTexture, "textures/earth.jpg");

	Scene* scene = new Scene("Cornell Box", *g_GfxDevice);
	g_ActiveScene = scene;

	const entity_id light = scene->add_entity("Light");
	ecs::add_component<Renderable>(light, Renderable{ g_PlaneModel.get_model() });
	ecs::get_component<Transform>(light)->position = { 0.0f, 9.9f, 0.0f };
	ecs::get_component<Transform>(light)->scale = 3.0f * glm::vec3(1.0f);
	ecs::get_component<Material>(light)->color = 20.0f * glm::vec3(1.0f);
	ecs::get_component<Material>(light)->type = Material::Type::DIFFUSE_LIGHT;

	const entity_id sphere = scene->add_entity("Sphere");
	ecs::add_component<Renderable>(sphere, Renderable{ g_Sphere.get() });
	ecs::get_component<Transform>(sphere)->position = { -2.0f, 1.5f, -2.0f };
	ecs::get_component<Material>(sphere)->color = { 1.0f, 1.0f, 1.0f };
	ecs::get_component<Material>(sphere)->roughness = 0.02f;
	ecs::get_component<Material>(sphere)->metallic = 0.0f;
	ecs::get_component<Material>(sphere)->albedoTexIndex =
		g_GfxDevice->get_descriptor_index(*g_TestTexture.get_texture(), SubresourceType::SRV);

	const entity_id lucy = scene->add_entity("Lucy");
	ecs::add_component<Renderable>(lucy, Renderable{ g_LucyModel.get_model() });
	ecs::get_component<Transform>(lucy)->position = { 1.0f, 0.0f, 2.0f };
	ecs::get_component<Transform>(lucy)->scale = glm::vec3(2.0f);
	ecs::get_component<Transform>(lucy)->orientation = glm::angleAxis(glm::radians(120.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	ecs::get_component<Material>(lucy)->color = { 1.0f, 1.0f, 1.0f };
	ecs::get_component<Material>(lucy)->roughness = 0.3f;
	ecs::get_component<Material>(lucy)->metallic = 1.0f;

	const entity_id floor = scene->add_entity("Floor");
	ecs::add_component<Renderable>(floor, Renderable{ g_PlaneModel.get_model() });
	ecs::get_component<Transform>(floor)->position = { 0.0f, 0.0f, 0.0f };
	ecs::get_component<Transform>(floor)->scale = glm::vec3(10.0f);
	ecs::get_component<Material>(floor)->color = { 0.5f, 0.5f, 0.5f };
	ecs::get_component<Material>(floor)->roughness = 0.001f;

	const entity_id backWall = scene->add_entity("Back Wall");
	ecs::add_component<Renderable>(backWall, Renderable{ g_PlaneModel.get_model() });
	ecs::get_component<Transform>(backWall)->position = { 0.0f, 5.0f, 5.0f };
	ecs::get_component<Transform>(backWall)->orientation = glm::angleAxis(-glm::half_pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f));
	ecs::get_component<Transform>(backWall)->scale = glm::vec3(10.0f);
	ecs::get_component<Material>(backWall)->color = { 0.7f, 0.7f, 1.0f };
	ecs::get_component<Material>(backWall)->roughness = 0.0f;
	ecs::get_component<Material>(backWall)->metallic = 1.0f;

	const entity_id leftWall = scene->add_entity("Left Wall");
	ecs::add_component<Renderable>(leftWall, Renderable{ g_PlaneModel.get_model() });
	ecs::get_component<Transform>(leftWall)->position = { -5.0f, 5.0f, 0.0f };
	ecs::get_component<Transform>(leftWall)->orientation = glm::angleAxis(-glm::half_pi<float>(), glm::vec3(0.0f, 0.0f, 1.0f));
	ecs::get_component<Transform>(leftWall)->scale = glm::vec3(10.0f);
	ecs::get_component<Material>(leftWall)->color = { 0.6f, 0.0f, 0.0f };

	const entity_id rightWall = scene->add_entity("Right Wall");
	ecs::add_component<Renderable>(rightWall, Renderable{ g_PlaneModel.get_model() });
	ecs::get_component<Transform>(rightWall)->position = { 5.0f, 5.0f, 0.0f };
	ecs::get_component<Transform>(rightWall)->orientation = glm::angleAxis(glm::half_pi<float>(), glm::vec3(0.0f, 0.0f, 1.0f));
	ecs::get_component<Transform>(rightWall)->scale = glm::vec3(10.0f);
	ecs::get_component<Material>(rightWall)->color = { 0.0f, 0.6f, 0.0f };

	const entity_id ceiling = scene->add_entity("Ceiling");
	ecs::add_component<Renderable>(ceiling, Renderable{ g_PlaneModel.get_model() });
	ecs::get_component<Transform>(ceiling)->position = { 0.0f, 10.0f, 0.0f };
	ecs::get_component<Transform>(ceiling)->scale = glm::vec3(10.0f);
	ecs::get_component<Material>(ceiling)->color = { 1.0f, 1.0f, 1.0f };

	g_RayTracingPass->initialize(*scene);
}

INTERNAL void create_sponza_scene() {
	Scene* scene = new Scene("Sponza", *g_GfxDevice);
	g_ActiveScene = scene;

	assetmanager::load_from_file(g_PlaneModel, "models/thin_plane/thin_plane.gltf");
	assetmanager::load_from_file(g_SponzaModel, "models/sponza/sponza.gltf");

	//const entity_id light = scene->add_entity("Light");
	//ecs::add_component<Renderable>(light, Renderable{ g_PlaneModel.get_model() });
	//ecs::get_component<Transform>(light)->position = { 0.0f, 9.9f, 0.0f };
	//ecs::get_component<Transform>(light)->scale = 3.0f * glm::vec3(1.0f);
	//ecs::get_component<Material>(light)->color = 20.0f * glm::vec3(1.0f);
	//ecs::get_component<Material>(light)->type = Material::Type::DIFFUSE_LIGHT;

	const entity_id sponza = scene->add_entity("Sponza");
	ecs::add_component<Renderable>(sponza, Renderable{ g_SponzaModel.get_model() });
	ecs::get_component<Transform>(sponza)->position = { 0.0f, 0.0f, 0.0f };
	ecs::get_component<Material>(sponza)->color = { 1.0f, 1.0f, 1.0f };
	ecs::get_component<Material>(sponza)->roughness = 0.5f;

	g_RayTracingPass->initialize(*scene);
}

INTERNAL void init_objects() {
	assetmanager::initialize(*g_GfxDevice);
	ecs::initialize();

	g_Camera = std::make_unique<Camera>(
		glm::vec3(0, 3.0f, -4.0f),
		glm::angleAxis(glm::radians(0.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
		60.0f,
		(float)g_FrameWidth / g_FrameHeight,
		0.1f,
		100.0f
	);
}

INTERNAL void on_update(FrameInfo& frameInfo) {
	// ------------------------------- Main Menu -------------------------------
	g_UIPass->begin_menu_bar(frameInfo.width);
	{
		// File menu
		if (g_UIPass->begin_menu("File")) {
			if (g_UIPass->begin_menu("New")) {
				g_UIPass->menu_item("Scene");
			}
			g_UIPass->end_menu();

			if (g_UIPass->begin_menu("Load Demo Scene")) {
				if (g_UIPass->menu_item("Cornell Box")) {
					if (g_ActiveScene == nullptr ||
						g_ActiveScene->get_name() != "Cornell Box") {

						std::cout << "Loading Cornell Box...\n";
						create_cornell_scene();
					}
				}
				g_UIPass->menu_item("Pool Table");
			}
			g_UIPass->end_menu();

			g_UIPass->menu_item("Save Scene");
			g_UIPass->menu_item("Save Scene As");
		}
		g_UIPass->end_menu();

		// Edit menu
		if (g_UIPass->begin_menu("Edit")) {
			g_UIPass->menu_item("Preferences");
		}
		g_UIPass->end_menu();

		// View menu
		if (g_UIPass->begin_menu("View")) {
			g_UIPass->menu_item("Renderpasses");
		}
		g_UIPass->end_menu();
	}
	g_UIPass->end_menu_bar();

	// ------------------------- Path Tracing Settings -------------------------
	// Samples per pixel (stratified sampling)
	LOCAL_PERSIST uint32_t samplesPerPixelRoot = 1;
	g_UIPass->widget_text("Path Tracing:");

	if (g_UIPass->widget_button("<") && samplesPerPixelRoot > 1) {
		--samplesPerPixelRoot;
	}
	g_UIPass->widget_same_line();
	g_UIPass->widget_text(std::to_string(samplesPerPixelRoot * samplesPerPixelRoot), 50);
	g_UIPass->widget_same_line();
	if (g_UIPass->widget_button(">")) {
		++samplesPerPixelRoot;
	}

	g_UIPass->widget_same_line();
	g_UIPass->widget_text("Samples Per Pixel (stratified)");
	
	// Camera settings
	LOCAL_PERSIST float fov = g_Camera->get_vertical_fov();
	if (g_UIPass->widget_slider_float("FOV", &fov, 10.0f, 110.0f)) {
		g_Camera->set_vertical_fov(fov);
	}

	// Input
	input::update();
	input::MouseState mouse = {};
	input::get_mouse_state(mouse);

	Camera& camera = *frameInfo.camera;
	const float cameraMoveSpeed = 5.0f;
	const float mouseSensitivity = 0.001f;

	if (mouse.buttons[2]) {
		glm::quat newOrientation = camera.get_orientation();

		if (mouse.deltaY != 0.0f) {
			newOrientation = newOrientation * glm::angleAxis(mouse.deltaY * mouseSensitivity, glm::vec3(1.0f, 0.0f, 0.0f)); // pitch
		}

		if (mouse.deltaX != 0.0f) {
			newOrientation = glm::angleAxis(mouse.deltaX * mouseSensitivity, glm::vec3(0.0f, 1.0f, 0.0f)) * newOrientation; // yaw
		}

		camera.set_orientation(newOrientation);
	}

	const glm::vec3 qRight = camera.get_right();
	const glm::vec3 qUp = camera.get_up();
	const glm::vec3 qForward = camera.get_forward();

	glm::vec3 newPosition = camera.get_position();

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

	camera.set_position(newPosition);

	const float aspectRatio = static_cast<float>(g_FrameWidth) / g_FrameHeight;
	camera.set_aspect_ratio(aspectRatio);

	camera.update();

	g_PerFrameData.projectionMatrix = camera.get_proj_matrix();
	g_PerFrameData.viewMatrix = camera.get_view_matrix();
	g_PerFrameData.invViewProjection = camera.get_inv_view_proj_matrix();
	g_PerFrameData.cameraPosition = camera.get_position();

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
	//create_cornell_scene();
	create_sponza_scene();

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
