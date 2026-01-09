#include "Core/Logger.h"
#include "Core/Types.h"
#include "Core/StringTypes.h"
#include "Core/Window.h"
#include "Core/System/Time.h"
#include "Data/ArenaAllocator.h"
#include "Data/Camera.h"
#include "Data/ComponentTypes.h"
#include "Data/Model.h"
#include "Data/Scene.h"
#include "Data/Font.h"
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
#include <assert.h>

#define DEFAULT_WIDTH  1920
#define DEFAULT_HEIGHT 1080

struct alignas(256) PerFrameData {
	glm::mat4 view;
	glm::mat4 proj;
	glm::mat4 inv_view;
	glm::mat4 inv_proj;
};

#define MAX_UI_DRAW_INSTANCES 16384
struct UIDrawInstance {
	glm::vec2 pos;
	glm::vec2 size;
	glm::vec2 texcoord_tl;
	glm::vec2 texcoord_br;
	glm::vec3 color;
	SRDescriptorIndex tex_index;
};

struct UIPassData {
	SRPipeline pipeline;
	SRShader vertex_shader;
	SRShader pixel_shader;
	SRBuffer draw_instance_buffers[SR_GFX_FRAMES_IN_FLIGHT];
	SRVector<UIDrawInstance> draw_instances_data;

	struct PushConstants {
		f32 inv_screen_width;
		f32 inv_screen_height;
		SRDescriptorIndex draw_intance_buffer_index;
	} push;
};

global SRLogger& logger = SRLogger::get(); // NOTE: Trick to ensure that logger outlives everything
global SRArena* g_arena;
global SRWindow* g_window;
global SRRenderGraph* g_render_graph;
global SRShaderCompiler* g_shader_compiler;
global SRFontLoader* g_font_loader;
global SREditor* g_editor;
global SRScene* g_scene;
global SRCamera g_camera;
global SRRenderPass* g_depth_prepass;
global SRRenderPass* g_composition_pass;
global SRRenderPass* g_gbuffer_pass;
global SRRenderPass* g_ui_pass;
global SRRenderPass* g_imgui_pass;
global SRGFXBackend g_gfx_backend = SRGFXBackend::DX12;
global SRGFXDevice g_gfx_device;
global SRModel g_test_model;
global SRBuffer g_per_frame_buffers[SR_GFX_FRAMES_IN_FLIGHT];
global PerFrameData g_per_frame_data;
global SRSwapchain g_swapchain;
global SRSampler g_sampler_linear;
global SRFont g_font;

internal void UIPass_Initialize(SRRenderPass& self, SRGFXDevice& gfxDevice, SRShaderCompiler& shaderCompiler);
internal void UIPass_OnExecute(SRRenderPass& self, SRGFXDevice& gfxDevice, const SRCmdList& cmdList, const SRFrameInfo& frameInfo);
internal void UIPass_OnDestroy(SRRenderPass& self, SRGFXDevice& gfxDevice);
internal void UIPass_DrawText(SRRenderPass& self, Str8 str);

internal void Window_OnResize(SRWindow* window, u32 new_width, u32 new_height);
internal void init_console();
internal void init_window();
internal void init_graphics();
internal void init_resources();
internal void init_scene();
internal void init_rendergraph();
internal void update(const SRFrameInfo* frameInfo);
internal void render(const SRFrameInfo* frameInfo);

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
	#ifdef _DEBUG
		init_console();
	#endif

	g_arena = SRArena_Create(Megabytes(512));

	init_window();
	init_graphics();
	init_resources();
	init_scene();
	init_rendergraph();
	SRGFX_FlushInitialUploads(&g_gfx_device); // TEMPORARY but important for now

	u32 window_width;
	u32 window_height;
	SRWindow_GetClientSize(g_window, &window_width, &window_height);

	SRFrameInfo frame_info = {
		.camera = &g_camera,
		.scene = g_scene,
		.dt = 0.0f,
		.width = window_width,
		.height = window_height
	};

	SRInput::initialize(g_window);
	SRTime::initialize();

	// Main loop
	bool is_first_frame = true;
	while (SRWindow_PollEvents(g_window)) {
		u32 window_width;
		u32 window_height;
		SRWindow_GetClientSize(g_window, &window_width, &window_height);

		SRTime::begin_frame();
		SRGFX_BeginFrame(&g_gfx_device, &g_swapchain);
		frame_info.perFrameBuffer = &g_per_frame_buffers[SRGFX_GetFrameIndex(&g_gfx_device)];
		frame_info.dt = (f32)SRTime::get_delta_sec();
		frame_info.width = window_width;
		frame_info.height = window_height;

		update(&frame_info);
		render(&frame_info);

		if (is_first_frame) {
			SRWindow_Show(g_window);
			is_first_frame = false;
		}
	}
	SRGFX_WaitForGPU(&g_gfx_device);

	SRFontLoader_Destroy(g_font_loader);
	SRShaderCompiler_Destroy(g_shader_compiler);

	// TODO: Temporary destruction logic, we will get rid of this eventually
	for (u32 f = 0; f < SR_GFX_FRAMES_IN_FLIGHT; ++f) { 
		SRGFX_DestroyResource(&g_gfx_device, &g_per_frame_buffers[f]);
	}

	auto* depthPrepassData = g_depth_prepass->get_pass_data<DepthPrepassData>();
	auto* gBufferPassData = g_gbuffer_pass->get_pass_data<GBufferPassData>();
	auto* ui_pass_data = g_ui_pass->get_pass_data<UIPassData>();
	auto* compositionPassData = g_composition_pass->get_pass_data<CompositionPassData>();

	SRGFX_DestroyPipeline(&g_gfx_device, &depthPrepassData->pipeline);
	SRGFX_DestroyPipeline(&g_gfx_device, &gBufferPassData->pipeline);
	SRGFX_DestroyPipeline(&g_gfx_device, &compositionPassData->pipeline);
	SRGFX_DestroyResource(&g_gfx_device, &g_test_model.vertexBuffer);
	SRGFX_DestroyResource(&g_gfx_device, &g_test_model.indexBuffer);
	SRGFX_DestroyResource(&g_gfx_device, &g_test_model.meshletBuffer);
	SRGFX_DestroyResource(&g_gfx_device, &g_test_model.meshletVerticesBuffer);
	SRGFX_DestroyResource(&g_gfx_device, &g_test_model.meshletTrianglesBuffer);
	SRGFX_DestroyResource(&g_gfx_device, &g_sampler_linear);
	SRGFX_DestroyResource(&g_gfx_device, &g_font.atlas_tex);
	UIPass_OnDestroy(*g_ui_pass, g_gfx_device);
	SRGFX_DestroySwapchain(&g_gfx_device, &g_swapchain);

	delete g_scene;
	delete g_editor;
	delete g_render_graph;
	SRWindow_Destroy(g_window);
	SRGFX_DestroyDevice(&g_gfx_device);
	SRArena_Destroy(g_arena);

	return 0;
}

internal void Window_OnResize(SRWindow* window, u32 new_width, u32 new_height) {
	SRSwapchainInfo new_swapchain_info = g_swapchain.info;
	new_swapchain_info.width = new_width;
	new_swapchain_info.height = new_height;

	SRGFX_CreateSwapchain(&g_gfx_device, window, &new_swapchain_info, &g_swapchain);
	g_render_graph->notify_swapchain_resize(g_gfx_device, new_width, new_height);

	SRFrameInfo frame_info = {
		.camera = &g_camera,
		.scene = g_scene,
		.dt = (f32)SRTime::get_delta_sec(),
		.width = new_width,
		.height = new_height
	};

	SRTime::begin_frame();
	SRGFX_BeginFrame(&g_gfx_device, &g_swapchain);
	frame_info.perFrameBuffer = &g_per_frame_buffers[SRGFX_GetFrameIndex(&g_gfx_device)];
	frame_info.dt = (f32)SRTime::get_delta_sec();
	frame_info.width = new_width;
	frame_info.height = new_height;

	update(&frame_info);
	render(&frame_info);
}

internal void init_console() {
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

internal void init_window() {
	const char* title = (g_gfx_backend == SRGFXBackend::Vulkan ? "Stingray (Vulkan)" : "Stingray (DX12)");
	g_window = SRWindow_Create(title, DEFAULT_WIDTH, DEFAULT_HEIGHT, SRWindowFlags_Centered | SRWindowFlags_SizeIsClientArea);

	SRWindow_SetOnResizeCallback(g_window, Window_OnResize);
}

internal void init_graphics() {
	SRGFX_CreateDevice(g_window, &g_gfx_device, g_gfx_backend);
	g_shader_compiler = SRShaderCompiler_Create(g_arena, g_gfx_backend);

	u32 window_width;
	u32 window_height;
	SRWindow_GetClientSize(g_window, &window_width, &window_height);

	SRSwapchainInfo swapchainInfo = {
		.width = window_width,
		.height = window_height,
		.numBuffers = 3,
		.format = SRFormat::RGBA8_UNORM,
		.vSync = true
	};
	SRGFX_CreateSwapchain(&g_gfx_device, g_window, &swapchainInfo, &g_swapchain);
	g_editor = new SREditor(*g_window, g_gfx_device, g_gfx_backend);

	// Samplers
	SRSamplerInfo linearSamplerInfo = {
		.filter = SRFilter::MinMagMipLinear,
		.addressU = SRTextureAddressMode::Wrap,
		.addressV = SRTextureAddressMode::Wrap,
		.addressW = SRTextureAddressMode::Wrap
	};
	SRGFX_CreateSampler(&g_gfx_device, &linearSamplerInfo, &g_sampler_linear);
}

internal void init_resources() {
	SRBufferInfo perFrameBufferInfo = {
		.size = sizeof(PerFrameData),
		.stride = sizeof(PerFrameData),
		.usage = SRUsage::Upload,
		.bindFlags = SRBindFlag::ConstantBuffer
	};

	for (u32 f = 0; f < SR_GFX_FRAMES_IN_FLIGHT; ++f) {
		SRGFX_CreateBuffer(&g_gfx_device, &perFrameBufferInfo, &g_per_frame_buffers[f], &g_per_frame_data);
	}

	SRModelLoader::load_gltf(RES_DIR "Models/StanfordBunny/StanfordBunny.gltf", g_test_model, g_gfx_device);

	g_font_loader = SRFontLoader_Create(g_arena);
	SRFontLoader_LoadFontFromSystem(g_font_loader, &g_gfx_device, "SegoeUI", 16, &g_font);
}

internal void init_scene() {
	g_scene = new SRScene(g_gfx_device, 65536);

	SREntityID entity = g_scene->add_entity();
	g_scene->add_component<SRTransform>(entity, SRTransform{});
	g_scene->add_component<SRRenderable>(entity, SRRenderable{ &g_test_model });
	g_camera = {
		.position = glm::vec3(0.0f, 0.1f, -0.3f),
		.orientation = glm::angleAxis(glm::radians(0.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
		.vertical_fov = 60.0f,
		.z_near = 0.01f,
		.z_far = 20.0f
	};
}

internal void init_rendergraph() {
	g_render_graph = new SRRenderGraph();

	u32 window_width;
	u32 window_height;
	SRWindow_GetClientSize(g_window, &window_width, &window_height);

	g_depth_prepass = &g_render_graph->add_render_pass("DepthPrepass", SRPassType::Graphics)
		.add_depth_output("Depth", window_width, window_height, SRFormat::D32_FLOAT)
		.set_execute_callback(SRDepthPrepass::execute);
	SRDepthPrepass::build(*g_depth_prepass, g_gfx_device, *g_shader_compiler);

	g_gbuffer_pass = &g_render_graph->add_render_pass("GBufferPass", SRPassType::Graphics)
		.add_depth_input("Depth")
		.add_color_output("GBufferAlbedo", window_width, window_height, SRFormat::RGBA8_UNORM)
		.set_execute_callback(SRGBufferPass::execute);
	SRGBufferPass::build(*g_gbuffer_pass, g_gfx_device, *g_shader_compiler);

	g_composition_pass = &g_render_graph->add_render_pass("CompositionPass", SRPassType::Graphics)
		.add_color_input("GBufferAlbedo", SRAccessFlag::Read)
		.set_execute_callback(SRCompositionPass::execute);
	SRCompositionPass::build(*g_composition_pass, g_gfx_device, *g_shader_compiler);

	// TODO: Update render graph to respect mesh shading pipeline
	//auto& meshletPass = g_render_graph->add_render_pass("MeshletPass", SRPassType::Graphics)
	//	.set_execute_callback(SRMeshletGenerationpass::execute);
	//SRMeshletGenerationpass::build(meshletPass, g_gfx_device, *g_shader_compiler);

	g_ui_pass = &g_render_graph->add_render_pass("UIPass", SRPassType::Graphics)
		.set_execute_callback(UIPass_OnExecute);
	UIPass_Initialize(*g_ui_pass, g_gfx_device, *g_shader_compiler);
	g_imgui_pass = &g_render_graph->add_render_pass("ImGuiPass", SRPassType::Graphics)
		.set_execute_callback([&](SRRenderPass& self, SRGFXDevice& gfxDevice, const SRCmdList& cmdList, const SRFrameInfo& frameInfo) {
			g_editor->update(*g_render_graph);
			g_editor->render(cmdList);
		});

	g_render_graph->build(g_gfx_device);
}

internal void update(const SRFrameInfo* frameInfo) {
	SRInput::update();
	SRMouseState mouse = SRInput::get_mouse_state();

	SRCamera* cam = frameInfo->camera;
	f32 cam_move_speed = 0.9f;
	f32 mouse_sensitivity = 0.001f;
	f32 dx = mouse_sensitivity * (f32)mouse.dx;
	f32 dy = mouse_sensitivity * (f32)mouse.dy;

	if (mouse.buttonStates & SRMouseButton_Middle) {
		glm::quat* orientation = &cam->orientation;
		*orientation = (*orientation) * glm::angleAxis(dy, glm::vec3(1.0f, 0.0f, 0.0f)); // pitch
		*orientation = glm::angleAxis(dx, glm::vec3(0.0f, 1.0f, 0.0f)) * (*orientation); // yaw
		*orientation = glm::normalize(*orientation);
	}

	glm::vec3 cam_right = SRCamera_GetRight(cam);
	glm::vec3 cam_up = SRCamera_GetUp(cam);
	glm::vec3 cam_forward = SRCamera_GetForward(cam);

	if (SRInput::is_key_down(SRKey_W)) {
		cam->position += cam_move_speed * frameInfo->dt * cam_forward;
	}
	if (SRInput::is_key_down(SRKey_A)) {
		cam->position -= cam_move_speed * frameInfo->dt * cam_right;
	}
	if (SRInput::is_key_down(SRKey_S)) {
		cam->position -= cam_move_speed * frameInfo->dt * cam_forward;
	}
	if (SRInput::is_key_down(SRKey_D)) {
		cam->position += cam_move_speed * frameInfo->dt * cam_right;
	}
	if (SRInput::is_key_down(SRKey_Space)) {
		cam->position.y += cam_move_speed * frameInfo->dt;
	}
	if (SRInput::is_key_down(SRKey_LeftControl)) {
		cam->position.y -= cam_move_speed * frameInfo->dt;
	}

	SRCamera_ComputeView(cam, &g_per_frame_data.view);
	SRCamera_ComputeProj(cam, SRWindow_GetClientAspectRatio(g_window), &g_per_frame_data.proj);
	g_per_frame_data.inv_view = glm::inverse(g_per_frame_data.view);
	g_per_frame_data.inv_proj = glm::inverse(g_per_frame_data.proj);

	std::memcpy(frameInfo->perFrameBuffer->mappedData, &g_per_frame_data, sizeof(g_per_frame_data));
}

internal void render(const SRFrameInfo* frame_info) {
	SRCmdList cmdList = SRGFX_BeginCommandList(&g_gfx_device, SRQueue_Universal);
	g_render_graph->execute(g_gfx_device, g_swapchain, cmdList, *frame_info);
	SRGFX_SubmitCommandLists(&g_gfx_device, &g_swapchain);
}

internal void UIPass_Initialize(SRRenderPass& self, SRGFXDevice& gfxDevice, SRShaderCompiler& shaderCompiler) {
	auto& pass_data = self.allocate_pass_data<UIPassData>();
	SRShaderCompiler_CompileFromFile(&shaderCompiler, RES_DIR "Shaders/UIPass.hlsl", { SRShaderStage::Vertex, "vertex_main" }, &pass_data.vertex_shader);
	SRShaderCompiler_CompileFromFile(&shaderCompiler, RES_DIR "Shaders/UIPass.hlsl", { SRShaderStage::Pixel, "pixel_main" }, &pass_data.pixel_shader);

	SRPipelineInfo pipelineInfo = {
		.vertexShader = &pass_data.vertex_shader,
		.pixelShader = &pass_data.pixel_shader,
		.blendState = {
			.alphaToCoverage = false,
			.independentBlend = false,
			.renderTargetBlendStates = {
				SRBlendState::RenderTargetBlendState {
					.blendEnable = true,
					.srcBlend = SRBlend::SrcAlpha,
					.dstBlend = SRBlend::InvSrcAlpha,
					.blendOp = SRBlendOp::Add,
					.srcBlendAlpha = SRBlend::One,
					.dstBlendAlpha = SRBlend::One,
					.blendOpAlpha = SRBlendOp::Add
				}
			}
		},
		.numRenderTargets = 1,
		.renderTargetFormats = { SRFormat::RGBA8_UNORM }
	};
	SRGFX_CreatePipeline(&gfxDevice, &pipelineInfo, &pass_data.pipeline);

	SRBufferInfo ui_draw_instance_buffer_info = {
		.size = MAX_UI_DRAW_INSTANCES * sizeof(UIDrawInstance),
		.stride = sizeof(UIDrawInstance),
		.usage = SRUsage::Upload,
		.bindFlags = SRBindFlag::ShaderResource,
		.miscFlags = SRMiscFlag::StructuredBuffer
	};
	for (u32 f = 0; f < SR_GFX_FRAMES_IN_FLIGHT; ++f) {
		SRGFX_CreateBuffer(&gfxDevice, &ui_draw_instance_buffer_info, &pass_data.draw_instance_buffers[f], nullptr);
	}

	SRVector_Create(&pass_data.draw_instances_data);
}

internal void UIPass_OnExecute(SRRenderPass& self, SRGFXDevice& gfxDevice, const SRCmdList& cmdList, const SRFrameInfo& frameInfo) {
	auto* pass_data = self.get_pass_data<UIPassData>();
	u32 frame_index = SRGFX_GetFrameIndex(&gfxDevice);

	// Update
	// TODO: Move elsewhere
	UIPass_DrawText(self, Str8_Literal("Hello World! Stingray-RT Version 0.2"));

	memcpy(
		pass_data->draw_instance_buffers[frame_index].mappedData,
		pass_data->draw_instances_data.data,
		pass_data->draw_instances_data.size * sizeof(UIDrawInstance)
	);
	//

	SRViewport viewport = {
		.width = static_cast<f32>(frameInfo.width),
		.height = static_cast<f32>(frameInfo.height),
	};
	pass_data->push.inv_screen_width = 1.0f / viewport.width;
	pass_data->push.inv_screen_height = 1.0f / viewport.height;
	pass_data->push.draw_intance_buffer_index = SRGFX_GetDescriptorIndexSRV(&gfxDevice, &pass_data->draw_instance_buffers[frame_index]);

	SRGFX_BindViewport(&gfxDevice, &viewport, &cmdList);
	SRGFX_BindPipeline(&gfxDevice, &pass_data->pipeline, &cmdList);
	SRGFX_PushConstants(&gfxDevice, &pass_data->push, sizeof(pass_data->push), &cmdList);

	assert(pass_data->draw_instances_data.size > 0);
	SRGFX_DrawInstanced(&gfxDevice, 6, (u32)pass_data->draw_instances_data.size, 0, 0, &cmdList);
	
	SRVector_Clear(&pass_data->draw_instances_data);
}

internal void UIPass_OnDestroy(SRRenderPass& self, SRGFXDevice& gfxDevice) {
	auto* pass_data = self.get_pass_data<UIPassData>();
	SRVector_Destroy(&pass_data->draw_instances_data);

	SRGFX_DestroyPipeline(&gfxDevice, &pass_data->pipeline);
	for (u32 f = 0; f < SR_GFX_FRAMES_IN_FLIGHT; ++f) {
		SRGFX_DestroyResource(&gfxDevice, &pass_data->draw_instance_buffers[f]);
	}
}

void UIPass_DrawText(SRRenderPass& self, Str8 str) {
	auto* pass_data = self.get_pass_data<UIPassData>();

	f32 text_pos_x = 300;
	f32 text_pos_y = 20;
	SRDescriptorIndex tex_index = SRGFX_GetDescriptorIndexSRV(&g_gfx_device, &g_font.atlas_tex);

	for (u64 i = 0; i < str.size; ++i) {
		u8 c = str.data[i];
		SRFontGlyph* glyph = &g_font.glyphs[c];

		if (c == ' ') {
			text_pos_x += glyph->advance_x;
		}

		// TODO: perhaps we don't need to store bearingX??
		UIDrawInstance draw_instance = {
			.pos = { text_pos_x + (f32)glyph->bearing_x, text_pos_y + (f32)g_font.bbox_ymax - (f32)glyph->bearing_y },
			.size = { glyph->width, glyph->height },
			.texcoord_tl = glyph->atlas_tex_coord_tl,
			.texcoord_br = glyph->atlas_tex_coord_br,
			.color = { 1.0f, 1.0f, 1.0f },
			.tex_index = tex_index
		};
		SRVector_PushBack(&pass_data->draw_instances_data, draw_instance);

		text_pos_x += glyph->advance_x;
	}
}


