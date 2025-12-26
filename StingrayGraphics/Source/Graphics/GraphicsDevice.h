#pragma once

#include "Core/Types.h"
#include "Core/Window.h"
#include "Graphics/GraphicsTypes.h"

#define SR_GFX_FRAMES_IN_FLIGHT 2

// NEW API
enum struct SRGFXBackend {
	DX12,
	Vulkan
};
struct SRGFXDevice;

typedef void(*SRGFX_DestroyDeviceFunc)(SRGFXDevice*);
typedef u32(*SRGFX_GetFrameIndexFunc)(SRGFXDevice*);
typedef void (*SRGFX_CreateSwapchainFunc)(SRGFXDevice*, const SRSwapchainInfo*, SRSwapchain*);
typedef void (*SRGFX_CreatePipelineFunc)(SRGFXDevice*, const SRPipelineInfo*, SRPipeline*);
typedef void (*SRGFX_CreateBufferFunc)(SRGFXDevice*, const SRBufferInfo*, SRBuffer*, const void*);
typedef void (*SRGFX_CreateTextureFunc)(SRGFXDevice*, const SRTextureInfo*, SRTexture*, const SRSubresourceData*);
typedef void (*SRGFX_CreateSamplerFunc)(SRGFXDevice*, const SRSamplerInfo*, SRSampler*);
typedef void (*SRGFX_BindPipelineFunc)(SRGFXDevice*, const SRPipeline*, const SRCmdList*);
typedef void (*SRGFX_BindViewportFunc)(SRGFXDevice*, const SRViewport*, const SRCmdList*);
typedef void (*SRGFX_BindVertexBufferFunc)(SRGFXDevice*, const SRBuffer*, const SRCmdList*);
typedef void (*SRGFX_BindIndexBufferFunc)(SRGFXDevice*, const SRBuffer*, const SRCmdList*);
typedef void (*SRGFX_BindRootConstantBufferFunc)(SRGFXDevice*, const SRBuffer*, const SRCmdList*);
typedef void (*SRGFX_PushConstantsFunc)(SRGFXDevice*, const void*, u32, const SRCmdList*);
typedef void (*SRGFX_BarrierFunc)(SRGFXDevice*, const SRBarrier*, u32, const SRCmdList*);
typedef void (*SRGFX_BeginFrameFunc)(SRGFXDevice*, const SRSwapchain*);
typedef SRCmdList(*SRGFX_BeginCommandListFunc)(SRGFXDevice*, SRQueue);
typedef void (*SRGFX_BeginRenderPassSwapchainFunc)(SRGFXDevice*, const SRSwapchain*, const SRCmdList*);
typedef void (*SRGFX_BeginRenderPassFunc)(SRGFXDevice*, const SRPassInfo*, const SRCmdList*);
typedef void (*SRGFX_EndRenderPassSwapchainFunc)(SRGFXDevice*, const SRSwapchain*, const SRCmdList*);
typedef void (*SRGFX_EndRenderPassFunc)(SRGFXDevice*, const SRCmdList*);
typedef void (*SRGFX_SubmitCommandListsFunc)(SRGFXDevice*, const SRSwapchain*);
typedef void (*SRGFX_DrawFunc)(SRGFXDevice*, u32, u32, const SRCmdList*);
typedef void (*SRGFX_DrawIndexedFunc)(SRGFXDevice*, u32, u32, u32, const SRCmdList*);
typedef void (*SRGFX_DispatchMeshFunc)(SRGFXDevice*, u32, u32, u32, const SRCmdList*);
typedef SRDescriptorIndex(*SRGFX_GetDescriptorIndexSRVFunc)(SRGFXDevice*, const SRResource*);
typedef SRShaderPlatformInfo(*SRGFX_GetShaderPlatformInfoFunc)(SRGFXDevice*);
typedef void (*SRGFX_WaitForGPUFunc)(SRGFXDevice*);
typedef void (*SRGFX_FlushInitialUploadsFunc)(SRGFXDevice*);
typedef void (*SRGFX_SetupImGuiInitInfoFunc)(SRGFXDevice*, SRFormat);

struct SRGFXDeviceVTable {
	SRGFX_DestroyDeviceFunc            destroy_device;
	SRGFX_GetFrameIndexFunc            get_frame_index;
	SRGFX_CreateSwapchainFunc          create_swapchain;
	SRGFX_CreatePipelineFunc           create_pipeline;
	SRGFX_CreateBufferFunc             create_buffer;
	SRGFX_CreateTextureFunc            create_texture;
	SRGFX_CreateSamplerFunc            create_sampler;
	SRGFX_BindPipelineFunc             bind_pipeline;
	SRGFX_BindViewportFunc             bind_viewport;
	SRGFX_BindVertexBufferFunc         bind_vertex_buffer;
	SRGFX_BindIndexBufferFunc          bind_index_buffer;
	SRGFX_BindRootConstantBufferFunc   bind_root_constant_buffer;
	SRGFX_PushConstantsFunc            push_constants;
	SRGFX_BarrierFunc                  barrier;
	SRGFX_BeginFrameFunc               begin_frame;
	SRGFX_BeginCommandListFunc         begin_command_list;
	SRGFX_BeginRenderPassSwapchainFunc begin_render_pass_swapchain;
	SRGFX_BeginRenderPassFunc          begin_render_pass;
	SRGFX_EndRenderPassSwapchainFunc   end_render_pass_swapchain;
	SRGFX_EndRenderPassFunc            end_render_pass;
	SRGFX_SubmitCommandListsFunc       submit_command_lists;
	SRGFX_DrawFunc                     draw;
	SRGFX_DrawIndexedFunc              draw_indexed;
	SRGFX_DispatchMeshFunc             dispatch_mesh;
	SRGFX_GetDescriptorIndexSRVFunc    get_descriptor_index_srv;
	SRGFX_GetShaderPlatformInfoFunc    get_shader_platform_info;
	SRGFX_WaitForGPUFunc               wait_for_gpu;
	SRGFX_FlushInitialUploadsFunc      flush_initial_uploads;
	SRGFX_SetupImGuiInitInfoFunc       setup_imgui_init_info;
};

struct SRGFXDevice {
	void* internalState;
	SRGFXDeviceVTable* vtbl;
};

void SRGFX_CreateDevice(const SRWindow* window, SRGFXDevice* device, SRGFXBackend backend);
void SRGFX_DestroyDevice(SRGFXDevice* device);

u32  SRGFX_GetFrameIndex(SRGFXDevice* device);

// Resource creation
void SRGFX_CreateSwapchain(SRGFXDevice* device, const SRWindow* window, const SRSwapchainInfo* info, SRSwapchain* swapchain);
void SRGFX_CreatePipeline(SRGFXDevice* device, const SRPipelineInfo* info, SRPipeline* pipeline);
void SRGFX_CreateBuffer(SRGFXDevice* device, const SRBufferInfo* info, SRBuffer* buffer, const void* data);
void SRGFX_CreateTexture(SRGFXDevice* device, const SRTextureInfo* info, SRTexture* texture, const SRSubresourceData* data);
void SRGFX_CreateSampler(SRGFXDevice* device, const SRSamplerInfo* info, SRSampler* sampler);

// Binding
void SRGFX_BindPipeline(SRGFXDevice* device, const SRPipeline* pipeline, const SRCmdList* cmdList);
void SRGFX_BindViewport(SRGFXDevice* device, const SRViewport* viewport, const SRCmdList* cmdList);
void SRGFX_BindVertexBuffer(SRGFXDevice* device, const SRBuffer* buffer, const SRCmdList* cmdList);
void SRGFX_BindIndexBuffer(SRGFXDevice* device, const SRBuffer* buffer, const SRCmdList* cmdList);
void SRGFX_BindRootConstantBuffer(SRGFXDevice* device, const SRBuffer* buffer, const SRCmdList* cmdList);
void SRGFX_PushConstants(SRGFXDevice* device, const void* data, u32 size, const SRCmdList* cmdList);
void SRGFX_Barrier(SRGFXDevice* device, const SRBarrier* barriers, u32 numBarriers, const SRCmdList* cmdList);

// Command lists / passes
void SRGFX_BeginFrame(SRGFXDevice* device, const SRSwapchain* swapchain);
SRCmdList SRGFX_BeginCommandList(SRGFXDevice* device, SRQueue queue);
void SRGFX_BeginRenderPassSwapchain(SRGFXDevice* device, const SRSwapchain* swapchain, const SRCmdList* cmdList);
void SRGFX_BeginRenderPass(SRGFXDevice* device, const SRPassInfo* passInfo, const SRCmdList* cmdList);
void SRGFX_EndRenderPassSwapchain(SRGFXDevice* device, const SRSwapchain* swapchain, const SRCmdList* cmdList);
void SRGFX_EndRenderPass(SRGFXDevice* device, const SRCmdList* cmdList);
void SRGFX_SubmitCommandLists(SRGFXDevice* device, const SRSwapchain* swapchain);

// Draw
void SRGFX_Draw(SRGFXDevice* device, u32 vtxCount, u32 startVtx, const SRCmdList* cmdList);
void SRGFX_DrawIndexed(SRGFXDevice* device, u32 idxCount, u32 startIdx, u32 baseVtx, const SRCmdList* cmdList);
void SRGFX_DispatchMesh(SRGFXDevice* device, u32 x, u32 y, u32 z, const SRCmdList* cmdList);

// Misc
SRDescriptorIndex SRGFX_GetDescriptorIndexSRV(SRGFXDevice* device, const SRResource* resource);
SRShaderPlatformInfo SRGFX_GetShaderPlatformInfo(SRGFXDevice* device);
void SRGFX_WaitForGPU(SRGFXDevice* device);
void SRGFX_FlushInitialUploads(SRGFXDevice* device);
void SRGFX_SetupImGuiInitInfo(SRGFXDevice* device, SRFormat swapchainFormat);

// OLD API
//class SRGraphicsDevice {
//public:
//	SRGraphicsDevice(SRWindow& window) : m_Window(window) {};
//	virtual ~SRGraphicsDevice() = default;
//
//	virtual u32 get_frame_index() const = 0;
//
//	virtual void create_swapchain(const SRSwapchainInfo& info, SRSwapchain& swapchain) = 0;
//	virtual void create_pipeline(const SRPipelineInfo& info, SRPipeline& pipeline) = 0;
//	virtual void create_buffer(const SRBufferInfo& info, SRBuffer& buffer, const void* data) = 0;
//	virtual void create_texture(const SRTextureInfo& info, SRTexture& texture, const SRSubresourceData* data) = 0;
//	virtual void create_sampler(const SRSamplerInfo& info, SRSampler& sampler) = 0;
//
//	virtual void bind_pipeline(const SRPipeline& pipeline, const SRCmdList& cmdList) = 0;
//	virtual void bind_viewport(const SRViewport& viewport, const SRCmdList& cmdList) = 0;
//	virtual void bind_vertex_buffer(const SRBuffer& buffer, const SRCmdList& cmdList) = 0;
//	virtual void bind_index_buffer(const SRBuffer& buffer, const SRCmdList& cmdList) = 0;
//	virtual void bind_root_constant_buffer(const SRBuffer& buffer, const SRCmdList& cmdList) = 0;
//	virtual void push_constants(const void* data, u32 size, const SRCmdList& cmdList) = 0;
//	virtual void barrier(const SRBarrier* pBarriers, u32 numBarriers, const SRCmdList& cmdList) = 0;
//
//	virtual void begin_frame(const SRSwapchain& swapchain) = 0;
//	virtual SRCmdList begin_command_list(SRQueue quee) = 0;
//	virtual void begin_render_pass(const SRSwapchain& swapchain, const SRCmdList& cmdList) = 0;
//	virtual void begin_render_pass(const SRPassInfo& passInfo, const SRCmdList& cmdList) = 0;
//	virtual void end_render_pass(const SRSwapchain& swapchain, const SRCmdList& cmdList) = 0;
//	virtual void end_render_pass(const SRCmdList& cmdList) = 0;
//	virtual void submit_command_lists(const SRSwapchain& swapchain) = 0;
//
//	virtual void draw(u32 vtxCount, u32 startVtx, const SRCmdList& cmdList) = 0;
//	virtual void draw_indexed(u32 idxCount, u32 startIdx, u32 baseVtx, const SRCmdList& cmdList) = 0;
//	virtual void dispatch_mesh(u32 groupCountX, u32 groupCountY, u32 groupCountZ, const SRCmdList& cmdList) = 0;
//
//	virtual SRDescriptorIndex get_descriptor_index_srv(const SRResource& resource) = 0;
//	virtual SRShaderPlatformInfo get_shader_platform_info() = 0;
//	virtual void wait_for_gpu() = 0;
//	virtual void flush_initial_uploads() = 0; // NOTE: TEMPORARY function, will be removed once we introduce streaming system
//	virtual void setup_imgui_init_info(SRFormat swapchainFormat) = 0;
//
//	static constexpr u32 FRAMES_IN_FLIGHT = 2;
//	
//protected:
//	SRWindow& m_Window;
//};
