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
#include "Math/MathFunctions.h"
#include "UI/UICore.h"

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

struct UIDrawInstance {
	glm::vec2 pos;
	glm::vec2 size;
	glm::vec2 texcoord_tl;
	glm::vec2 texcoord_br;
	glm::vec4 color;
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
global SRScene* g_scene;
global SRCamera g_camera;
global SRRenderPass* g_depth_prepass;
global SRRenderPass* g_composition_pass;
global SRRenderPass* g_gbuffer_pass;
global SRRenderPass* g_ui_pass;
global SRGFXBackend g_gfx_backend = SRGFXBackend::DX12;
global SRGFXDevice g_gfx_device;
global SRModel g_test_model;
global SRBuffer g_per_frame_buffers[SR_GFX_FRAMES_IN_FLIGHT];
global PerFrameData g_per_frame_data;
global SRSwapchain g_swapchain;
global SRSampler g_sampler_linear;
global SRFont g_font;
global UIContext* g_ui_ctx;
global u32 g_rand_state = 17;

internal void UI_RenderPass_GenerateDrawInstances(SRRenderPass& self, UINode* node);

internal void UIPass_Initialize(SRRenderPass& self, SRGFXDevice& gfx_device, SRShaderCompiler& shader_compiler);
internal void UIPass_OnExecute(SRRenderPass& self, SRGFXDevice& gfx_device, SRCmdList cmd_list, const SRFrameInfo* frame_info);
internal void UIPass_OnDestroy(SRRenderPass& self, SRGFXDevice& gfx_device);
internal void UIPass_DrawRect(SRRenderPass& self, glm::vec2 pos, f32 width, f32 height, glm::vec4 col);
internal void UIPass_DrawText(SRRenderPass& self, Str8 str, glm::vec2 pos, glm::vec4 col);

internal void Window_Initialize();
internal void Window_OnResize(SRWindow* window, u32 new_width, u32 new_height);

internal void Console_Initialize();
internal void App_InitializeGraphics();
internal void App_InitializeResources();
internal void App_InitializeScene();
internal void App_InitializeRendergraph();
internal void App_OnUpdate(const SRFrameInfo* frame_info);
internal void App_OnRender(const SRFrameInfo* frame_info);

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
	#ifdef _DEBUG
		Console_Initialize();
	#endif

	g_arena = SRArena_Create(Megabytes(512));

	Window_Initialize();
	App_InitializeGraphics();
	App_InitializeResources();
	g_ui_ctx = UI_CreateContext(&g_font);
	App_InitializeScene();
	App_InitializeRendergraph();
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

		App_OnUpdate(&frame_info);
		App_OnRender(&frame_info);

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

	UI_DestroyContext(g_ui_ctx);

	delete g_scene;
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

	SRGFX_CreateSwapchain(&g_gfx_device, new_swapchain_info, &g_swapchain);
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

	App_OnUpdate(&frame_info);
	App_OnRender(&frame_info);
}

internal void Console_Initialize() {
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

internal void Window_Initialize() {
	Str8 title;
	switch (g_gfx_backend) {
		case SRGFXBackend::Vulkan: { title = Str8_Literal("Stingray (Vulkan)"); } break;
		case SRGFXBackend::DX12: { title = Str8_Literal("Stingray (DX12)"); } break;
	}

	g_window = SRWindow_Create(g_arena, title, DEFAULT_WIDTH, DEFAULT_HEIGHT, SRWindowFlags_Centered | SRWindowFlags_SizeIsClientArea);

	SRWindow_SetOnResizeCallback(g_window, Window_OnResize);
}

internal void App_InitializeGraphics() {
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
	SRGFX_CreateSwapchain(&g_gfx_device, swapchainInfo, &g_swapchain);

	// Samplers
	SRSamplerInfo linearSamplerInfo = {
		.filter = SRFilter::MinMagMipLinear,
		.addressU = SRTextureAddressMode::Wrap,
		.addressV = SRTextureAddressMode::Wrap,
		.addressW = SRTextureAddressMode::Wrap
	};
	SRGFX_CreateSampler(&g_gfx_device, linearSamplerInfo, &g_sampler_linear);
}

internal void App_InitializeResources() {
	SRBufferInfo perFrameBufferInfo = {
		.size = sizeof(PerFrameData),
		.stride = sizeof(PerFrameData),
		.usage = SRUsage::Upload,
		.bindFlags = SRBindFlag::ConstantBuffer
	};

	for (u32 f = 0; f < SR_GFX_FRAMES_IN_FLIGHT; ++f) {
		SRGFX_CreateBuffer(&g_gfx_device, perFrameBufferInfo, &g_per_frame_buffers[f], &g_per_frame_data);
	}

	SRModelLoader::load_gltf(RES_DIR "Models/StanfordBunny/StanfordBunny.gltf", g_test_model, g_gfx_device);

	g_font_loader = SRFontLoader_Create(g_arena);
	SRFontLoader_LoadFontFromSystem(g_font_loader, &g_gfx_device, "SegoeUI", 15, &g_font);
}

internal void App_InitializeScene() {
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

internal void App_InitializeRendergraph() {
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

	g_render_graph->build(g_gfx_device);
}

internal void App_OnUpdate(const SRFrameInfo* frame_info) {
	SRInput::update();
	SRMouseState mouse = SRInput::get_mouse_state();

	SRCamera* cam = frame_info->camera;
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
		cam->position += cam_move_speed * frame_info->dt * cam_forward;
	}
	if (SRInput::is_key_down(SRKey_A)) {
		cam->position -= cam_move_speed * frame_info->dt * cam_right;
	}
	if (SRInput::is_key_down(SRKey_S)) {
		cam->position -= cam_move_speed * frame_info->dt * cam_forward;
	}
	if (SRInput::is_key_down(SRKey_D)) {
		cam->position += cam_move_speed * frame_info->dt * cam_right;
	}
	if (SRInput::is_key_down(SRKey_Space)) {
		cam->position.y += cam_move_speed * frame_info->dt;
	}
	if (SRInput::is_key_down(SRKey_LeftControl)) {
		cam->position.y -= cam_move_speed * frame_info->dt;
	}

	SRCamera_ComputeView(cam, &g_per_frame_data.view);
	SRCamera_ComputeProj(cam, SRWindow_GetClientAspectRatio(g_window), &g_per_frame_data.proj);
	g_per_frame_data.inv_view = glm::inverse(g_per_frame_data.view);
	g_per_frame_data.inv_proj = glm::inverse(g_per_frame_data.proj);

	std::memcpy(frame_info->perFrameBuffer->mappedData, &g_per_frame_data, sizeof(g_per_frame_data));
}

internal void App_OnRender(const SRFrameInfo* frame_info) {
	SRCmdList cmd_list = SRGFX_BeginCommandList(&g_gfx_device, SRQueue_Universal);
	g_render_graph->execute(g_gfx_device, g_swapchain, cmd_list, frame_info);
	SRGFX_SubmitCommandLists(&g_gfx_device, &g_swapchain);
}

internal void UI_RenderPass_GenerateDrawInstances(SRRenderPass& self, UINode* node) {
	assert(node);

	if (has_flag(node->flags, UINodeFlags::DrawBackground)) {
		UIPass_DrawRect(
			self,
			{ node->computed_pos_rel[UIAxis_X], node->computed_pos_rel[UIAxis_Y] },
			node->computed_size[UIAxis_X],
			node->computed_size[UIAxis_Y],
			glm::vec4(Rand_F32(&g_rand_state), Rand_F32(&g_rand_state), Rand_F32(&g_rand_state), 0.5f)
		);
	}
	if (has_flag(node->flags, UINodeFlags::DrawText)) {
		UIPass_DrawText(self, node->str, { node->computed_pos_rel[UIAxis_X], node->computed_pos_rel[UIAxis_Y] }, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
	}
	if (has_flag(node->flags, UINodeFlags::Clickable)) {
		// TODO
	}

	UINode* child = node->first_child;
	while (child != nullptr) {
		UI_RenderPass_GenerateDrawInstances(self, child);
		child = child->next_sibling;
	}
}

internal void UIPass_Initialize(SRRenderPass& self, SRGFXDevice& gfx_device, SRShaderCompiler& shader_compiler) {
	auto& pass_data = self.allocate_pass_data<UIPassData>();
	SRShaderCompiler_CompileFromFile(&shader_compiler, Str8_Literal(RES_DIR "Shaders/UIPass.hlsl"), { SRShaderStage::Vertex, Str8_Literal("vertex_main") }, &pass_data.vertex_shader);
	SRShaderCompiler_CompileFromFile(&shader_compiler, Str8_Literal(RES_DIR "Shaders/UIPass.hlsl"), { SRShaderStage::Pixel, Str8_Literal("pixel_main") }, &pass_data.pixel_shader);

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
	SRGFX_CreatePipeline(&gfx_device, &pipelineInfo, &pass_data.pipeline);

	SRBufferInfo ui_draw_instance_buffer_info = {
		.size = MAX_UI_DRAW_INSTANCES * sizeof(UIDrawInstance),
		.stride = sizeof(UIDrawInstance),
		.usage = SRUsage::Upload,
		.bindFlags = SRBindFlag::ShaderResource,
		.miscFlags = SRMiscFlag::StructuredBuffer
	};
	for (u32 f = 0; f < SR_GFX_FRAMES_IN_FLIGHT; ++f) {
		SRGFX_CreateBuffer(&gfx_device, ui_draw_instance_buffer_info, &pass_data.draw_instance_buffers[f], nullptr);
	}

	SRVector_Create(&pass_data.draw_instances_data);
}

internal void UIPass_OnExecute(SRRenderPass& self, SRGFXDevice& gfx_device, SRCmdList cmd_list, const SRFrameInfo* frame_info) {
	auto* pass_data = self.get_pass_data<UIPassData>();
	u32 frame_index = SRGFX_GetFrameIndex(&gfx_device);

	UI_BeginFrame((f32)frame_info->width, (f32)frame_info->height);
	{
		UINode* title_bar = UI_BeginRow(Str8_Literal("Title Bar"), { UISizeType::PercentOfParent, 1.0f }, { UISizeType::Pixels, 34 });
		{
			UI_Button(Str8_Literal("File"));
			UI_Button(Str8_Literal("Edit"));
			UI_Button(Str8_Literal("View"));
			UI_Button(Str8_Literal("Tools"));
			UI_Button(Str8_Literal("Window"));
			UI_Button(Str8_Literal("Help"));
		}
		UI_EndRow();

		UINode* main_content = UI_BeginRow(Str8_Literal("Main Content"), { UISizeType::PercentOfParent, 1.0f }, { UISizeType::Pixels, 600 });
		{
			UINode* left_layout = UI_BeginCol(Str8_Literal("Left Layout"), { UISizeType::PercentOfParent, 0.2f }, { UISizeType::Pixels, 300 });
			{
				UI_Button(Str8_Literal("Button in the left layout"));
			}
			UI_EndCol();

			UINode* right_layout = UI_BeginCol(Str8_Literal("Right Layout"), { UISizeType::PercentOfParent, 0.8f }, { UISizeType::Pixels, 300 });
			{
				UI_Button(Str8_Literal("Button in the right layout"));
			}
			UI_EndCol();
		}
		UI_EndRow();
	}
	UI_EndFrame();
	
	// TODO: Move UI rendering logic elsewhere?
	UI_RenderPass_GenerateDrawInstances(self, UI_GetRootNode());
	memcpy(
		pass_data->draw_instance_buffers[frame_index].mappedData,
		pass_data->draw_instances_data.data,
		pass_data->draw_instances_data.size * sizeof(UIDrawInstance)
	);

	SRViewport viewport = {
		.width = static_cast<f32>(frame_info->width),
		.height = static_cast<f32>(frame_info->height),
	};
	pass_data->push.inv_screen_width = 1.0f / viewport.width;
	pass_data->push.inv_screen_height = 1.0f / viewport.height;
	pass_data->push.draw_intance_buffer_index = SRGFX_GetDescriptorIndexSRV(&gfx_device, pass_data->draw_instance_buffers[frame_index]);

	SRGFX_BindViewport(&gfx_device, &viewport, cmd_list);
	SRGFX_BindPipeline(&gfx_device, &pass_data->pipeline, cmd_list);
	SRGFX_PushConstants(&gfx_device, &pass_data->push, sizeof(pass_data->push), cmd_list);

	assert(pass_data->draw_instances_data.size > 0);
	SRGFX_DrawInstanced(&gfx_device, 6, (u32)pass_data->draw_instances_data.size, 0, 0, cmd_list);
	
	SRVector_Clear(&pass_data->draw_instances_data);
	g_rand_state = 17;
}

internal void UIPass_OnDestroy(SRRenderPass& self, SRGFXDevice& gfx_device) {
	auto* pass_data = self.get_pass_data<UIPassData>();
	SRVector_Destroy(&pass_data->draw_instances_data);

	SRGFX_DestroyPipeline(&gfx_device, &pass_data->pipeline);
	for (u32 f = 0; f < SR_GFX_FRAMES_IN_FLIGHT; ++f) {
		SRGFX_DestroyResource(&gfx_device, &pass_data->draw_instance_buffers[f]);
	}

}

internal void UIPass_DrawRect(SRRenderPass& self, glm::vec2 pos, f32 width, f32 height, glm::vec4 col) {
	auto* pass_data = self.get_pass_data<UIPassData>();

	UIDrawInstance draw_instance = {
		.pos = pos,
		.size = { width, height },
		.color = col,
		.tex_index = SR_INVALID_DESCRIPTOR_INDEX // Note: Shader will just use input color directly
	};
	SRVector_PushBack(&pass_data->draw_instances_data, draw_instance);
}

internal void UIPass_DrawText(SRRenderPass& self, Str8 str, glm::vec2 pos, glm::vec4 col) {
	auto* pass_data = self.get_pass_data<UIPassData>();
	SRDescriptorIndex tex_index = SRGFX_GetDescriptorIndexSRV(&g_gfx_device, g_font.atlas_tex);

	for (u64 i = 0; i < str.size; ++i) {
		u8 c = str.data[i];
		SRFontGlyph* glyph = &g_font.glyphs[c];

		if (c == ' ') {
			pos.x += (f32)glyph->advance_x;
			continue;
		}

		UIDrawInstance draw_instance = {
			.pos = { pos.x + (f32)glyph->bearing_x, pos.y + (f32)g_font.bearing_ymax - (f32)glyph->bearing_y },
			.size = { glyph->width, glyph->height },
			.texcoord_tl = glyph->atlas_tex_coord_tl,
			.texcoord_br = glyph->atlas_tex_coord_br,
			.color = col,
			.tex_index = tex_index
		};
		SRVector_PushBack(&pass_data->draw_instances_data, draw_instance);

		pos.x += static_cast<f32>(glyph->advance_x);
	}
}
