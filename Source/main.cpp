#include "Core/FrameInfo.h"
#include "Core/Platform.h"
#include "Core/Window.h"
#include "Data/Camera.h"
#include "Data/Scene.h"
#include "ECS/ECS.h"
#include "Graphics/RenderGraph.h"
#include "Graphics/Renderpasses/FullscreenTriPass.h"
#include "Graphics/Renderpasses/RayTracingPass.h"
#include "Graphics/Renderpasses/UIPass.h"
#include "Graphics/Vulkan/GraphicsDeviceVulkan.h"
#include "Input/Input.h"
#include "Managers/AssetManager.h"
#include "Managers/MaterialManager.h"

#include <chrono>
#include <cstdint>
#include <format>
#include <glm/glm.hpp>
#include <iostream>
#include <memory>
#include <vector>
#include <Windows.h>

using namespace SR;

enum class UIState : uint8_t {
	HIDDEN,
	VISIBLE
};

struct PerFrameData {
	glm::mat4 projectionMatrix = { 1.0f };
	glm::mat4 viewMatrix = { 1.0f };
	glm::mat4 invViewProjection = { 1.0f };
	glm::vec3 cameraPosition = { 0.0f, 0.0f, 0.0f };
	uint32_t pad1 = 0;
};

GLOBAL GraphicsAPI g_API = GraphicsAPI::VULKAN;
GLOBAL UIState g_UIState = UIState::VISIBLE;
GLOBAL UIEvent g_MouseEvent = UIEvent(UIEventType::None);
GLOBAL UIEvent g_KeyboardEvent = UIEvent(UIEventType::None);

GLOBAL std::unique_ptr<Window> g_Window = {};
GLOBAL std::unique_ptr<GraphicsDevice> g_GfxDevice = {};
GLOBAL std::unique_ptr<RenderGraph> g_RenderGraph = {};
GLOBAL std::unique_ptr<FullscreenTriPass> g_FullscreenTriPass = {};
GLOBAL std::unique_ptr<RayTracingPass> g_RayTracingPass = {};
GLOBAL std::unique_ptr<UIPass> g_UIPass = {};
GLOBAL SwapChain g_SwapChain = {};
GLOBAL Sampler g_DefaultSampler = {};

GLOBAL std::unique_ptr<MaterialManager> g_MaterialManager = {};
GLOBAL Buffer g_PerFrameDataBuffers[GraphicsDevice::FRAMES_IN_FLIGHT] = {};
GLOBAL PerFrameData g_PerFrameData = {};

GLOBAL std::unique_ptr<Camera> g_Camera = {};
GLOBAL FrameInfo g_FrameInfo = {};
GLOBAL uint64_t g_LastFrameCount = 0;
GLOBAL uint64_t g_CurrentFPS = 0;
GLOBAL std::chrono::high_resolution_clock::time_point g_FPSStartTime = {};
GLOBAL Scene* g_ActiveScene = nullptr;

// Resources
GLOBAL Texture g_DefaultAlbedoMap = {};
GLOBAL Texture g_DefaultNormalMap = {};
GLOBAL Asset g_EarthTexture = {};
GLOBAL Asset g_PlaneModel = {};
GLOBAL Asset g_LucyModel = {};
GLOBAL Asset g_SponzaModel = {};
GLOBAL std::unique_ptr<Model> g_FlatPlaneModel = {};
GLOBAL std::unique_ptr<Model> g_Sphere = {};

// --------------------------- Function Declarations ---------------------------
INTERNAL void init_window();
INTERNAL void init_gfx();
INTERNAL void init_objects();
INTERNAL void init_render_graph();
INTERNAL void create_cornell_scene();
INTERNAL void create_sponza_scene();
INTERNAL void on_update(FrameInfo& frameInfo);
INTERNAL void resize_callback(int width, int height);
INTERNAL void mouse_position_callback(int x, int y);
INTERNAL void mouse_button_callback(MouseButton button, ButtonAction action, ButtonMods mods);
INTERNAL void key_callback(Key keys, ButtonAction action, ButtonMods mods);

// -------------------------------- Entry Point --------------------------------
int APIENTRY wWinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR lpCmdLine,
	_In_ int nCmdShow)
{
	init_window();
	init_gfx();
	init_objects();
	init_render_graph();
	create_cornell_scene();
	//create_sponza_scene();

	g_FrameInfo.camera = g_Camera.get();

	bool firstFrame = true;
	while (!g_Window->should_close()) {
		g_Window->poll_events();

		// Extra security check
		if (g_Window->should_close()) {
			break;
		}

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
		g_FrameInfo.width = g_Window->get_client_width();
		g_FrameInfo.height = g_Window->get_client_height();
		on_update(g_FrameInfo);

		// Rendering
		CommandList cmdList = g_GfxDevice->begin_command_list(QueueType::DIRECT);
		static bool builtASes = false;

		if (!builtASes) {
			g_RayTracingPass->build_acceleration_structures(cmdList);
			builtASes = true;
		}

		g_RenderGraph->execute(g_SwapChain, cmdList, g_FrameInfo);
		g_GfxDevice->submit_command_lists(g_SwapChain);

		if (firstFrame) {
			g_Window->show();
			firstFrame = false;
		}
	}
	g_GfxDevice->wait_for_gpu();

	// Shutdown
	ECS::destroy();
	AssetManager::destroy();
	#if defined(_DEBUG)
		FreeConsole();
	#endif

	return 0;
}

// --------------------------- Function Definitions ----------------------------
INTERNAL void init_window() {
	#if defined(_DEBUG)
		AllocConsole();
		SetConsoleTitle(L"Stingray Debug Console");

		// Redirect standard output stream to a file
		FILE* new_stdout;
		freopen_s(&new_stdout, "CONOUT$", "w", stdout);
		std::cout.clear();

		// Redirect standard input stream
		FILE* new_stdin;
		freopen_s(&new_stdin, "CONIN$", "r", stdin);
		std::cin.clear();

		// Redirect standard error stream to a file
		FILE* new_stderr;
		freopen_s(&new_stderr, "CONOUT$", "w", stderr);
		std::cerr.clear();
		std::cout << "Console attached successfully!" << std::endl;
	#endif

	g_Window = std::make_unique<Window>(
		"Stingray",
		1920,
		1080,
		WindowFlag::CENTER |
		WindowFlag::SIZE_IS_CLIENT_AREA |
		WindowFlag::NO_TITLEBAR
	);
	g_Window->set_resize_callback(resize_callback);
	g_Window->set_mouse_pos_callback(mouse_position_callback);
	g_Window->set_mouse_button_callback(mouse_button_callback);
	g_Window->set_keyboard_callback(key_callback);
}

INTERNAL void init_gfx() {
	switch (g_API) {
	case GraphicsAPI::VULKAN:
		{
			g_GfxDevice = std::make_unique<GraphicsDeviceVulkan>(*g_Window);
		}
		break;
	}

	const SwapChainInfo swapChainInfo = {
		.width = static_cast<uint32_t>(g_Window->get_client_width()),
		.height = static_cast<uint32_t>(g_Window->get_client_height()),
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

	for (uint32_t i = 0; i < GraphicsDevice::FRAMES_IN_FLIGHT; ++i) {
		g_GfxDevice->create_buffer(
			perFrameDataBufferInfo,
			g_PerFrameDataBuffers[i],
			&g_PerFrameData
		);
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

INTERNAL void init_objects() {
	g_MaterialManager = std::make_unique<MaterialManager>(*g_GfxDevice, 1024);
	AssetManager::initialize(*g_GfxDevice, *g_MaterialManager);
	ECS::initialize();

	g_Camera = std::make_unique<Camera>(
		glm::vec3(0.0f, 3.0f, -4.0f),
		glm::angleAxis(glm::radians(0.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
		60.0f,
		(float)g_Window->get_client_width() / g_Window->get_client_height(),
		0.1f,
		100.0f
	);
}

INTERNAL void init_render_graph() {
	// Initialize passes
	g_RayTracingPass = std::make_unique<RayTracingPass>(*g_GfxDevice);
	g_FullscreenTriPass = std::make_unique<FullscreenTriPass>(*g_GfxDevice);
	g_UIPass = std::make_unique<UIPass>(*g_GfxDevice, *g_Window);

	// Create render graph
	const uint32_t uWidth = static_cast<uint32_t>(g_Window->get_client_width());
	const uint32_t uHeight = static_cast<uint32_t>(g_Window->get_client_height());
	const uint32_t uRTWidth = static_cast<uint32_t>(uWidth * 0.6f - 2 * UIPass::UI_PADDING);
	const uint32_t uRTHeight = static_cast<uint32_t>(uRTWidth * (9.0f / 16));

	g_RenderGraph = std::make_unique<RenderGraph>(*g_GfxDevice);

	auto rtPass = g_RenderGraph->add_pass("RayTracingPass");
	rtPass->add_output_attachment("RTOutput", AttachmentInfo{ uRTWidth, uRTHeight, AttachmentType::RW_TEXTURE, Format::R8G8B8A8_UNORM });
	rtPass->add_output_attachment("RTAccumulation", AttachmentInfo{ uRTWidth, uRTHeight, AttachmentType::RW_TEXTURE, Format::R32G32B32A32_FLOAT });
	rtPass->set_execute_callback([&](PassExecuteInfo& executeInfo) {
		if (g_ActiveScene != nullptr) {
			g_RayTracingPass->execute(executeInfo, *g_ActiveScene);
		}
	});

	//auto fullscreenTriPass = g_RenderGraph->add_pass("FullScreenTriPass");
	//fullscreenTriPass->add_input_attachment("RTOutput");
	//fullscreenTriPass->set_execute_callback([&](PassExecuteInfo& executeInfo) {
	//	g_FullscreenTriPass->execute(executeInfo);
	//});

	auto uiPass = g_RenderGraph->add_pass("UIPass");
	uiPass->add_input_attachment("RTOutput");
	uiPass->set_execute_callback([&](PassExecuteInfo& executeInfo) {
		if (g_UIState == UIState::VISIBLE) {
			g_UIPass->execute(executeInfo);
		}
	});

	g_RenderGraph->build();
}

// ------------------------------- Create Scenes -------------------------------
INTERNAL void create_cornell_scene() {
	Scene* scene = new Scene("Cornell Box", *g_GfxDevice);
	g_ActiveScene = scene;

	AssetManager::load_from_file(g_EarthTexture, "textures/earth.jpg");
	AssetManager::load_from_file(g_PlaneModel, "models/thin_plane/thin_plane.gltf");
	AssetManager::load_from_file(g_LucyModel, "models/lucy/lucy.gltf");
	g_Sphere = AssetManager::create_sphere(1.5f, 32, 64);

	const entity_id light = scene->add_entity("Light");
	ECS::add_component<Renderable>(light, Renderable{ g_PlaneModel.get_model() });
	ECS::get_component<Transform>(light)->position = { 0.0f, 9.9f, 0.0f };
	ECS::get_component<Transform>(light)->scale = 3.0f * glm::vec3(1.0f);
	ECS::add_component<Material>(light, Material{
		.color = 20.0f * glm::vec3(1.0f),
		.type = Material::Type::DIFFUSE_LIGHT
		});

	const entity_id sphere = scene->add_entity("Sphere");
	ECS::add_component<Renderable>(sphere, Renderable{ g_Sphere.get() });
	ECS::get_component<Transform>(sphere)->position = { -2.0f, 1.5f, -2.0f };
	ECS::add_component(sphere, Material{
		.color = { 1.0f, 1.0f, 1.0f },
		.albedoTexIndex = g_GfxDevice->get_descriptor_index(*g_EarthTexture.get_texture(), SubresourceType::SRV),
		.metallic = 0.0f,
		.roughness = 0.02f,
		});

	const entity_id lucy = scene->add_entity("Lucy");
	ECS::add_component<Renderable>(lucy, Renderable{ g_LucyModel.get_model() });
	ECS::get_component<Transform>(lucy)->position = { 1.0f, 0.0f, 2.0f };
	ECS::get_component<Transform>(lucy)->scale = glm::vec3(2.0f);
	ECS::get_component<Transform>(lucy)->orientation = glm::angleAxis(glm::radians(120.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	ECS::add_component<Material>(lucy, Material{
		.color = { 1.0f, 1.0f, 1.0f },
		.metallic = 1.0f,
		.roughness = 0.3f,
		});

	const entity_id floor = scene->add_entity("Floor");
	ECS::add_component<Renderable>(floor, Renderable{ g_PlaneModel.get_model() });
	ECS::get_component<Transform>(floor)->position = { 0.0f, 0.0f, 0.0f };
	ECS::get_component<Transform>(floor)->scale = glm::vec3(10.0f);
	ECS::add_component<Material>(floor, Material{
		.color = { 0.5f, 0.5f, 0.5f },
		.roughness = 0.001f
		});

	const entity_id backWall = scene->add_entity("Back Wall");
	ECS::add_component<Renderable>(backWall, Renderable{ g_PlaneModel.get_model() });
	ECS::get_component<Transform>(backWall)->position = { 0.0f, 5.0f, 5.0f };
	ECS::get_component<Transform>(backWall)->orientation = glm::angleAxis(-glm::half_pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f));
	ECS::get_component<Transform>(backWall)->scale = glm::vec3(10.0f);
	ECS::add_component<Material>(backWall, Material{
		.color = { 0.7f, 0.7f, 1.0f },
		.metallic = 1.0f,
		.roughness = 0.0f
		});

	const entity_id leftWall = scene->add_entity("Left Wall");
	ECS::add_component<Renderable>(leftWall, Renderable{ g_PlaneModel.get_model() });
	ECS::get_component<Transform>(leftWall)->position = { -5.0f, 5.0f, 0.0f };
	ECS::get_component<Transform>(leftWall)->orientation = glm::angleAxis(-glm::half_pi<float>(), glm::vec3(0.0f, 0.0f, 1.0f));
	ECS::get_component<Transform>(leftWall)->scale = glm::vec3(10.0f);
	ECS::add_component<Material>(leftWall, Material{
		.color = { 0.6f, 0.0f, 0.0f }
		});

	const entity_id rightWall = scene->add_entity("Right Wall");
	ECS::add_component<Renderable>(rightWall, Renderable{ g_PlaneModel.get_model() });
	ECS::get_component<Transform>(rightWall)->position = { 5.0f, 5.0f, 0.0f };
	ECS::get_component<Transform>(rightWall)->orientation = glm::angleAxis(glm::half_pi<float>(), glm::vec3(0.0f, 0.0f, 1.0f));
	ECS::get_component<Transform>(rightWall)->scale = glm::vec3(10.0f);
	ECS::add_component<Material>(rightWall, Material{
		.color = { 0.0f, 0.6f, 0.0f }
		});

	const entity_id ceiling = scene->add_entity("Ceiling");
	ECS::add_component<Renderable>(ceiling, Renderable{ g_PlaneModel.get_model() });
	ECS::get_component<Transform>(ceiling)->position = { 0.0f, 10.0f, 0.0f };
	ECS::get_component<Transform>(ceiling)->scale = glm::vec3(10.0f);
	ECS::add_component<Material>(ceiling, Material{
		.color = { 1.0f, 1.0f, 1.0f }
		});

	g_RayTracingPass->m_UseSkybox = false;
	g_RayTracingPass->initialize(*scene, *g_MaterialManager);
}

INTERNAL void create_sponza_scene() {
	Scene* scene = new Scene("Sponza", *g_GfxDevice);
	g_ActiveScene = scene;

	//g_Sphere = AssetManager::create_sphere(0.5f, 32, 64);
	AssetManager::load_from_file(g_SponzaModel, "models/sponza/sponza.gltf");

	//const entity_id light = scene->add_entity("Light");
	//ECS::add_component<Renderable>(light, Renderable{ g_Sphere.get() });
	//ECS::get_component<Transform>(light)->position = { 0.0f, 9.9f, 0.0f };
	//ECS::add_component<Material>(light, Material{
	//	.color = 20.0f * glm::vec3(1.0f),
	//	.type = Material::Type::DIFFUSE_LIGHT
	//});

	const entity_id sponza = scene->add_entity("Sponza");
	ECS::add_component<Renderable>(sponza, Renderable{ g_SponzaModel.get_model() });
	ECS::get_component<Transform>(sponza)->position = { 0.0f, 0.0f, 0.0f };

	g_RayTracingPass->m_UseSkybox = true;
	g_RayTracingPass->initialize(*scene, *g_MaterialManager);
}

INTERNAL void on_update(FrameInfo& frameInfo) {
	// Input
	Input::update();
	Camera& camera = *frameInfo.camera;
	const float cameraMoveSpeed = 5.0f;
	const float mouseSensitivity = 0.001f;

	if (Input::is_down(MouseButton::MIDDLE)) {
		glm::quat newOrientation = camera.get_orientation();

		const glm::ivec2 mouseDelta = Input::get_mouse_delta();
		if (mouseDelta.y != 0) {
			newOrientation = newOrientation * glm::angleAxis(mouseDelta.y * mouseSensitivity, glm::vec3(1.0f, 0.0f, 0.0f)); // pitch
		}

		if (mouseDelta.x != 0) {
			newOrientation = glm::angleAxis(mouseDelta.x * mouseSensitivity, glm::vec3(0.0f, 1.0f, 0.0f)) * newOrientation; // yaw
		}

		camera.set_orientation(newOrientation);
	}

	const glm::vec3 qRight = camera.get_right();
	const glm::vec3 qUp = camera.get_up();
	const glm::vec3 qForward = camera.get_forward();

	glm::vec3 newPosition = camera.get_position();

	if (Input::is_down(Key::W)) {
		newPosition += qForward * cameraMoveSpeed * frameInfo.dt;
	}
	if (Input::is_down(Key::A)) {
		newPosition -= qRight * cameraMoveSpeed * frameInfo.dt;
	}
	if (Input::is_down(Key::S)) {
		newPosition -= qForward * cameraMoveSpeed * frameInfo.dt;
	}
	if (Input::is_down(Key::D)) {
		newPosition += qRight * cameraMoveSpeed * frameInfo.dt;
	}

	if (Input::is_down(Key::SPACE)) {
		newPosition.y += cameraMoveSpeed * frameInfo.dt;
	}
	if (Input::is_down(Key::LEFT_CONTROL)) {
		newPosition.y -= cameraMoveSpeed * frameInfo.dt;
	}
	camera.set_position(newPosition);
	camera.set_aspect_ratio(16.0f / 9);
	camera.update();

	g_PerFrameData.projectionMatrix = camera.get_proj_matrix();
	g_PerFrameData.viewMatrix = camera.get_view_matrix();
	g_PerFrameData.invViewProjection = camera.get_inv_view_proj_matrix();
	g_PerFrameData.cameraPosition = camera.get_position();

	std::memcpy(g_PerFrameDataBuffers[g_GfxDevice->get_frame_index()].mappedData, &g_PerFrameData, sizeof(g_PerFrameData));

	// User interface
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

	g_UIPass->begin_split("MainLayout");
	{
		g_UIPass->begin_panel("Properties", 0.2f);
		{
			g_UIPass->widget_text("Path Tracing:");
			g_UIPass->widget_checkbox("Use normal maps", &g_RayTracingPass->m_UseNormalMaps);
			g_UIPass->widget_checkbox("Use skybox", &g_RayTracingPass->m_UseSkybox);

			LOCAL_PERSIST float fov = g_Camera->get_vertical_fov();
			if (g_UIPass->widget_slider_float("FOV", &fov, 10.0f, 110.0f)) {
				g_Camera->set_vertical_fov(fov);
			}
			g_UIPass->widget_text(std::format("FPS: {}", g_CurrentFPS));

			if (g_UIPass->widget_button("Reload")) {
				std::cout << "Reload\n";
			}
		}
		g_UIPass->end_panel();

		g_UIPass->begin_panel("3D View", 0.6f);
		{
			auto* rtOutputAttachment = g_RenderGraph->get_attachment("RTOutput");
			const Texture& rtOutputTex = rtOutputAttachment->texture;
			g_UIPass->widget_image(rtOutputTex, rtOutputTex.info.width, rtOutputTex.info.height);
		}
		g_UIPass->end_panel();

		g_UIPass->begin_panel("Output Log", 0.2f);
		{
		}
		g_UIPass->end_panel();
	}
	g_UIPass->end_split();
}

INTERNAL void resize_callback(int width, int height) {
	const SwapChainInfo swapChainInfo = {
		.width = static_cast<uint32_t>(width),
		.height = static_cast<uint32_t>(height),
		.bufferCount = 3,
		.format = Format::R8G8B8A8_UNORM,
		.vsync = true
	};

	width = static_cast<int>(width * 0.6f - 2 * UIPass::UI_PADDING);
	height = static_cast<int>(width * (9.0f / 16));
	// Recreate swapchain
	g_GfxDevice->create_swapchain(swapChainInfo, g_SwapChain);

	// Recreate resources with new sizes
	auto rtOutput = g_RenderGraph->get_attachment("RTOutput");
	auto rtAccumulation = g_RenderGraph->get_attachment("RTAccumulation");

	TextureInfo newRtOutputTexInfo = rtOutput->texture.info;
	newRtOutputTexInfo.width = static_cast<uint32_t>(width);
	newRtOutputTexInfo.height = static_cast<uint32_t>(height);
	rtOutput->info.width = newRtOutputTexInfo.width;
	rtOutput->info.height = newRtOutputTexInfo.height;

	TextureInfo newRtAccumulationTexInfo = rtAccumulation->texture.info;
	newRtAccumulationTexInfo.width = static_cast<uint32_t>(width);
	newRtAccumulationTexInfo.height = static_cast<uint32_t>(height);
	rtAccumulation->info.width = newRtAccumulationTexInfo.width;
	rtAccumulation->info.height = newRtAccumulationTexInfo.height;

	g_GfxDevice->create_texture(newRtOutputTexInfo, rtOutput->texture, nullptr);
	g_GfxDevice->create_texture(newRtAccumulationTexInfo, rtAccumulation->texture, nullptr);

	// Update the attachment infos in the render graph
	rtOutput->currentState = ResourceState::UNORDERED_ACCESS;
	rtAccumulation->currentState = ResourceState::UNORDERED_ACCESS;
}

INTERNAL void mouse_position_callback(int x, int y) {
	if (g_UIPass != nullptr) {
		g_MouseEvent.set_type(UIEventType::MouseMove);

		MouseEventData& mouse = g_MouseEvent.get_mouse_data();
		mouse.position.x = static_cast<float>(x);
		mouse.position.y = static_cast<float>(y);

		g_UIPass->process_event(g_MouseEvent);
	}
}

INTERNAL void mouse_button_callback(MouseButton button, ButtonAction action, ButtonMods mods) {
	if (g_UIPass != nullptr && g_UIState == UIState::VISIBLE) {
		MouseEventData& mouse = g_MouseEvent.get_mouse_data();
		g_MouseEvent.set_type(action == ButtonAction::PRESS ? UIEventType::MouseDown : UIEventType::MouseUp);

		if (button == MouseButton::MOUSE1) {
			mouse.downButtons.left = action == ButtonAction::PRESS;
		}
		else if (button == MouseButton::MOUSE2) {
			mouse.downButtons.right = action == ButtonAction::PRESS;
		}
		else if (button == MouseButton::MOUSE3) {
			mouse.downButtons.middle = action == ButtonAction::PRESS;
		}

		g_UIPass->process_event(g_MouseEvent);
	}
}

INTERNAL void key_callback(Key keys, ButtonAction action, ButtonMods mods) {
	if (g_UIPass != nullptr && g_UIState == UIState::VISIBLE) {
		//if (action == GLFW_PRESS) {
		//	g_KeyboardEvent.set_type(UIEventType::KeyboardDown);
		//}
		//else {
		//	g_KeyboardEvent.set_type(UIEventType::KeyboardUp);
		//}

		//KeyboardEventData& keyboard = g_KeyboardEvent.get_keyboard_data();
		//keyboard.key = key;
		//keyboard.action = action;
		//keyboard.mods = mods;

		//g_UIPass->process_event(g_KeyboardEvent);
	}
}

//INTERNAL void key_char_callback(GLFWwindow* window, unsigned int codepoint) {
//	if (g_UIPass != nullptr) {
//		UIEvent keyCharEvent(UIEventType::KeyboardChar);
//		KeyboardCharData& keyCharData = keyCharEvent.get_keyboard_char_data();
//		keyCharData.codePoint = codepoint;
//
//		g_UIPass->process_event(keyCharEvent);
//	}
//}
