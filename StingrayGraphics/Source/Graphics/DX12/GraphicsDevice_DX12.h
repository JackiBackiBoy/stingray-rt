#pragma once

#include "Graphics/GraphicsDevice.h"

void SRGFXDX12_CreateDevice(const SRWindow* window, SRGFXDevice* device);
void SRGFXDX12_DestroyDevice(SRGFXDevice* device);

u32  SRGFXDX12_GetFrameIndex(SRGFXDevice* device);

void SRGFXDX12_CreateSwapchain(SRGFXDevice* device, const SRSwapchainInfo* info, SRSwapchain* swapchain);
void SRGFXDX12_CreatePipeline(SRGFXDevice* device, const SRPipelineInfo* info, SRPipeline* pipeline);
void SRGFXDX12_CreateBuffer(SRGFXDevice* device, const SRBufferInfo* info, SRBuffer* buffer, const void* data);
void SRGFXDX12_CreateTexture(SRGFXDevice* device, const SRTextureInfo* info, SRTexture* texture, const SRSubresourceData* data);
void SRGFXDX12_CreateSampler(SRGFXDevice* device, const SRSamplerInfo* info, SRSampler* sampler);

void SRGFXDX12_BindPipeline(SRGFXDevice* device, const SRPipeline* pipeline, const SRCmdList* cmdList);
void SRGFXDX12_BindViewport(SRGFXDevice* device, const SRViewport* viewport, const SRCmdList* cmdList);
void SRGFXDX12_BindVertexBuffer(SRGFXDevice* device, const SRBuffer* buffer, const SRCmdList* cmdList);
void SRGFXDX12_BindIndexBuffer(SRGFXDevice* device, const SRBuffer* buffer, const SRCmdList* cmdList);
void SRGFXDX12_BindRootConstantBuffer(SRGFXDevice* device, const SRBuffer* buffer, const SRCmdList* cmdList);
void SRGFXDX12_PushConstants(SRGFXDevice* device, const void* data, u32 size, const SRCmdList* cmdList);
void SRGFXDX12_Barrier(SRGFXDevice* device, const SRBarrier* barriers, u32 numBarriers, const SRCmdList* cmdList);

void SRGFXDX12_BeginFrame(SRGFXDevice* device, const SRSwapchain* swapchain);
SRCmdList SRGFXDX12_BeginCommandList(SRGFXDevice* device, SRQueue queue);
void SRGFXDX12_BeginRenderPassSwapchain(SRGFXDevice* device, const SRSwapchain* swapchain, const SRCmdList* cmdList);
void SRGFXDX12_BeginRenderPass(SRGFXDevice* device, const SRPassInfo* passInfo, const SRCmdList* cmdList);
void SRGFXDX12_EndRenderPassSwapchain(SRGFXDevice* device, const SRSwapchain* swapchain, const SRCmdList* cmdList);
void SRGFXDX12_EndRenderPass(SRGFXDevice* device, const SRCmdList* cmdList);
void SRGFXDX12_SubmitCommandLists(SRGFXDevice* device, const SRSwapchain* swapchain);

void SRGFXDX12_Draw(SRGFXDevice* device, u32 vtxCount, u32 startVtx, const SRCmdList* cmdList);
void SRGFXDX12_DrawIndexed(SRGFXDevice* device, u32 idxCount, u32 startIdx, u32 baseVtx, const SRCmdList* cmdList);
void SRGFXDX12_DispatchMesh(SRGFXDevice* device, u32 x, u32 y, u32 z, const SRCmdList* cmdList);

SRDescriptorIndex SRGFXDX12_GetDescriptorIndexSRV(SRGFXDevice* device, const SRResource* resource);
SRShaderPlatformInfo SRGFXDX12_GetShaderPlatformInfo(SRGFXDevice* device);
void SRGFXDX12_WaitForGPU(SRGFXDevice* device);
void SRGFXDX12_FlushInitialUploads(SRGFXDevice* device);
void SRGFXDX12_SetupImGuiInitInfo(SRGFXDevice* device, SRFormat swapchainFormat);
