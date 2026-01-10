#pragma once

#include "Graphics/GraphicsDevice.h"

void SRGFXDX12_CreateDevice(SRWindow* window, SRGFXDevice* device);
void SRGFXDX12_DestroyDevice(SRGFXDevice* device);

u32  SRGFXDX12_GetFrameIndex(SRGFXDevice* device);

void SRGFXDX12_CreateSwapchain(SRGFXDevice* device, SRSwapchainInfo info, SRSwapchain* swapchain);
void SRGFXDX12_CreatePipeline(SRGFXDevice* device, const SRPipelineInfo* info, SRPipeline* pipeline);
void SRGFXDX12_CreateBuffer(SRGFXDevice* device, SRBufferInfo info, SRBuffer* buffer, const void* data);
void SRGFXDX12_CreateTexture(SRGFXDevice* device, const SRTextureInfo* info, SRTexture* texture, const SRSubresourceData* data);
void SRGFXDX12_CreateSampler(SRGFXDevice* device, SRSamplerInfo info, SRSampler* sampler);

void SRGFXDX12_DestroySwapchain(SRGFXDevice* device, SRSwapchain* swapchain);
void SRGFXDX12_DestroyPipeline(SRGFXDevice* device, SRPipeline* pipeline);
void SRGFXDX12_DestroyResource(SRGFXDevice* device, SRResource* resource);

void SRGFXDX12_BindPipeline(SRGFXDevice* device, const SRPipeline* pipeline, SRCmdList cmdList);
void SRGFXDX12_BindViewport(SRGFXDevice* device, const SRViewport* viewport, SRCmdList cmdList);
void SRGFXDX12_BindVertexBuffer(SRGFXDevice* device, const SRBuffer* buffer, SRCmdList cmdList);
void SRGFXDX12_BindIndexBuffer(SRGFXDevice* device, const SRBuffer* buffer, SRCmdList cmdList);
void SRGFXDX12_BindRootConstantBuffer(SRGFXDevice* device, const SRBuffer* buffer, SRCmdList cmdList);
void SRGFXDX12_PushConstants(SRGFXDevice* device, const void* data, u32 size, SRCmdList cmdList);
void SRGFXDX12_Barrier(SRGFXDevice* device, const SRBarrier* barriers, u32 numBarriers, SRCmdList cmdList);

void SRGFXDX12_BeginFrame(SRGFXDevice* device, const SRSwapchain* swapchain);
SRCmdList SRGFXDX12_BeginCommandList(SRGFXDevice* device, SRQueue queue);
void SRGFXDX12_BeginRenderPassSwapchain(SRGFXDevice* device, const SRSwapchain* swapchain, SRCmdList cmdList);
void SRGFXDX12_BeginRenderPass(SRGFXDevice* device, const SRPassInfo* passInfo, SRCmdList cmdList);
void SRGFXDX12_EndRenderPassSwapchain(SRGFXDevice* device, const SRSwapchain* swapchain, SRCmdList cmdList);
void SRGFXDX12_EndRenderPass(SRGFXDevice* device, SRCmdList cmdList);
void SRGFXDX12_SubmitCommandLists(SRGFXDevice* device, const SRSwapchain* swapchain);

void SRGFXDX12_Draw(SRGFXDevice* device, u32 vtxCount, u32 startVtx, SRCmdList cmdList);
void SRGFXDX12_DrawIndexed(SRGFXDevice* device, u32 idxCount, u32 startIdx, u32 baseVtx, SRCmdList cmdList);
void SRGFXDX12_DrawInstanced(SRGFXDevice* device, u32 vtx_count, u32 inst_count, u32 start_vtx, uint32_t start_inst, SRCmdList cmd_list);
void SRGFXDX12_DispatchMesh(SRGFXDevice* device, u32 x, u32 y, u32 z, SRCmdList cmd_list);

SRDescriptorIndex SRGFXDX12_GetDescriptorIndexSRV(SRGFXDevice* device, SRResource resource);
SRShaderCompileTarget SRGFXDX12_GetShaderCompileTarget(SRGFXDevice* device);
void SRGFXDX12_WaitForGPU(SRGFXDevice* device);
void SRGFXDX12_FlushInitialUploads(SRGFXDevice* device);
void SRGFXDX12_SetupImGuiInitInfo(SRGFXDevice* device, SRFormat swapchainFormat);
