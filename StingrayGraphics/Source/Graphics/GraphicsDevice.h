#pragma once

#include "Core/Types.h"
#include "Core/Window.h"
#include "Graphics/GraphicsTypes.h"

#define SR_GFX_FRAMES_IN_FLIGHT 2

struct SRGFXDevice;
typedef void (*SRGFX_DestroyDeviceFunc)(SRGFXDevice*);
typedef u32  (*SRGFX_GetFrameIndexFunc)(SRGFXDevice*);
typedef void (*SRGFX_CreateSwapchainFunc)(SRGFXDevice*, SRSwapchainInfo, SRSwapchain*);
typedef void (*SRGFX_CreatePipelineFunc)(SRGFXDevice*, const SRPipelineInfo*, SRPipeline*);
typedef void (*SRGFX_CreateBufferFunc)(SRGFXDevice*, SRBufferInfo, SRBuffer*, const void*);
typedef void (*SRGFX_CreateTextureFunc)(SRGFXDevice*, const SRTextureInfo*, SRTexture*, const SRSubresourceData*);
typedef void (*SRGFX_CreateSamplerFunc)(SRGFXDevice*, SRSamplerInfo, SRSampler*);
typedef void (*SRGFX_DestroySwapchainFunc)(SRGFXDevice*, SRSwapchain*);
typedef void (*SRGFX_DestroyPipelineFunc)(SRGFXDevice*, SRPipeline*);
typedef void (*SRGFX_DestroyResourceFunc)(SRGFXDevice*, SRResource*);
typedef void (*SRGFX_BindPipelineFunc)(SRGFXDevice*, const SRPipeline*, SRCmdList);
typedef void (*SRGFX_BindViewportFunc)(SRGFXDevice*, const SRViewport*, SRCmdList);
typedef void (*SRGFX_BindVertexBufferFunc)(SRGFXDevice*, const SRBuffer*, SRCmdList);
typedef void (*SRGFX_BindIndexBufferFunc)(SRGFXDevice*, const SRBuffer*, SRCmdList);
typedef void (*SRGFX_BindRootConstantBufferFunc)(SRGFXDevice*, const SRBuffer*, SRCmdList);
typedef void (*SRGFX_PushConstantsFunc)(SRGFXDevice*, const void*, u32, SRCmdList);
typedef void (*SRGFX_BarrierFunc)(SRGFXDevice*, const SRBarrier*, u32, SRCmdList);
typedef void (*SRGFX_BeginFrameFunc)(SRGFXDevice*, const SRSwapchain*);
typedef SRCmdList (*SRGFX_BeginCommandListFunc)(SRGFXDevice*, SRQueue);
typedef void (*SRGFX_BeginRenderPassSwapchainFunc)(SRGFXDevice*, const SRSwapchain*, SRCmdList);
typedef void (*SRGFX_BeginRenderPassFunc)(SRGFXDevice*, const SRPassInfo*, SRCmdList);
typedef void (*SRGFX_EndRenderPassSwapchainFunc)(SRGFXDevice*, const SRSwapchain*, SRCmdList);
typedef void (*SRGFX_EndRenderPassFunc)(SRGFXDevice*, SRCmdList);
typedef void (*SRGFX_SubmitCommandListsFunc)(SRGFXDevice*, const SRSwapchain*);
typedef void (*SRGFX_DrawFunc)(SRGFXDevice*, u32, u32, SRCmdList);
typedef void (*SRGFX_DrawIndexedFunc)(SRGFXDevice*, u32, u32, u32, SRCmdList);
typedef void (*SRGFX_DrawInstancedFunc)(SRGFXDevice*, u32, u32, u32, u32, SRCmdList);
typedef void (*SRGFX_DispatchMeshFunc)(SRGFXDevice*, u32, u32, u32, SRCmdList);
typedef SRDescriptorIndex (*SRGFX_GetDescriptorIndexSRVFunc)(SRGFXDevice*, SRResource);
typedef SRShaderCompileTarget (*SRGFX_GetShaderCompileTargetFunc)(SRGFXDevice*);
typedef void (*SRGFX_WaitForGPUFunc)(SRGFXDevice*);
typedef void (*SRGFX_FlushInitialUploadsFunc)(SRGFXDevice*);

struct SRGFXDeviceVTable {
	SRGFX_DestroyDeviceFunc            destroy_device;
	SRGFX_GetFrameIndexFunc            get_frame_index;
	SRGFX_CreateSwapchainFunc          create_swapchain;
	SRGFX_CreatePipelineFunc           create_pipeline;
	SRGFX_CreateBufferFunc             create_buffer;
	SRGFX_CreateTextureFunc            create_texture;
	SRGFX_CreateSamplerFunc            create_sampler;
	SRGFX_DestroySwapchainFunc         destroy_swapchain;
	SRGFX_DestroyPipelineFunc          destroy_pipeline;
	SRGFX_DestroyResourceFunc          destroy_resource;
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
	SRGFX_DrawInstancedFunc            draw_instanced;
	SRGFX_DispatchMeshFunc             dispatch_mesh;
	SRGFX_GetDescriptorIndexSRVFunc    get_descriptor_index_srv;
	SRGFX_GetShaderCompileTargetFunc   get_shader_compile_target;
	SRGFX_WaitForGPUFunc               wait_for_gpu;
	SRGFX_FlushInitialUploadsFunc      flush_initial_uploads;
};

struct SRGFXDevice {
	void* internalState;
	SRGFXDeviceVTable* vtbl;
};

void SRGFX_CreateDevice(SRWindow* window, SRGFXDevice* device, SRGFXBackend backend);
void SRGFX_DestroyDevice(SRGFXDevice* device);

u32  SRGFX_GetFrameIndex(SRGFXDevice* device);

void SRGFX_CreateSwapchain(SRGFXDevice* device, SRSwapchainInfo info, SRSwapchain* swapchain);
void SRGFX_CreatePipeline(SRGFXDevice* device, const SRPipelineInfo* info, SRPipeline* pipeline);
void SRGFX_CreateBuffer(SRGFXDevice* device, SRBufferInfo info, SRBuffer* buffer, const void* data);
void SRGFX_CreateTexture(SRGFXDevice* device, const SRTextureInfo* info, SRTexture* texture, const SRSubresourceData* data);
void SRGFX_CreateSampler(SRGFXDevice* device, SRSamplerInfo info, SRSampler* sampler);

void SRGFX_DestroySwapchain(SRGFXDevice* device, SRSwapchain* swapchain);
void SRGFX_DestroyPipeline(SRGFXDevice* device, SRPipeline* pipeline);
void SRGFX_DestroyResource(SRGFXDevice* device, SRResource* resource);

void SRGFX_BindPipeline(SRGFXDevice* device, const SRPipeline* pipeline, SRCmdList cmdList);
void SRGFX_BindViewport(SRGFXDevice* device, const SRViewport* viewport, SRCmdList cmdList);
void SRGFX_BindVertexBuffer(SRGFXDevice* device, const SRBuffer* buffer, SRCmdList cmdList);
void SRGFX_BindIndexBuffer(SRGFXDevice* device, const SRBuffer* buffer, SRCmdList cmdList);
void SRGFX_BindRootConstantBuffer(SRGFXDevice* device, const SRBuffer* buffer, SRCmdList cmdList);
void SRGFX_PushConstants(SRGFXDevice* device, const void* data, u32 size, SRCmdList cmdList);
void SRGFX_Barrier(SRGFXDevice* device, const SRBarrier* barriers, u32 numBarriers, SRCmdList cmdList);

void SRGFX_BeginFrame(SRGFXDevice* device, const SRSwapchain* swapchain);
SRCmdList SRGFX_BeginCommandList(SRGFXDevice* device, SRQueue queue);
void SRGFX_BeginRenderPassSwapchain(SRGFXDevice* device, const SRSwapchain* swapchain, SRCmdList cmdList);
void SRGFX_BeginRenderPass(SRGFXDevice* device, const SRPassInfo* passInfo, SRCmdList cmdList);
void SRGFX_EndRenderPassSwapchain(SRGFXDevice* device, const SRSwapchain* swapchain, SRCmdList cmdList);
void SRGFX_EndRenderPass(SRGFXDevice* device, SRCmdList cmdList);
void SRGFX_SubmitCommandLists(SRGFXDevice* device, const SRSwapchain* swapchain);

void SRGFX_Draw(SRGFXDevice* device, u32 vtxCount, u32 startVtx, SRCmdList cmdList);
void SRGFX_DrawIndexed(SRGFXDevice* device, u32 idxCount, u32 startIdx, u32 baseVtx, SRCmdList cmdList);
void SRGFX_DrawInstanced(SRGFXDevice* device, u32 vtx_count, u32 inst_count, u32 start_vtx, uint32_t start_inst, SRCmdList cmd_list);
void SRGFX_DispatchMesh(SRGFXDevice* device, u32 x, u32 y, u32 z, SRCmdList cmdList);

SRDescriptorIndex SRGFX_GetDescriptorIndexSRV(SRGFXDevice* device, SRResource resource);
SRShaderCompileTarget SRGFX_GetShaderCompileTarget(SRGFXDevice* device);
void SRGFX_WaitForGPU(SRGFXDevice* device);
void SRGFX_FlushInitialUploads(SRGFXDevice* device);
