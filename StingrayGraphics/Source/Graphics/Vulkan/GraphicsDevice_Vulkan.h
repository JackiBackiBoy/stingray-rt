#pragma once

#include "Graphics/GraphicsDevice.h"

void SRGFXVulkan_CreateDevice(SRWindow* window, SRGFXDevice* device);
void SRGFXVulkan_DestroyDevice(SRGFXDevice* device);

u32  SRGFXVulkan_GetFrameIndex(SRGFXDevice* device);

void SRGFXVulkan_CreateSwapchain(SRGFXDevice* device, SRSwapchainInfo info, SRSwapchain* swapchain);
void SRGFXVulkan_CreatePipeline(SRGFXDevice* device, const SRPipelineInfo* info, SRPipeline* pipeline);
void SRGFXVulkan_CreateBuffer(SRGFXDevice* device, SRBufferInfo info, SRBuffer* buffer, const void* data);
void SRGFXVulkan_CreateTexture(SRGFXDevice* device, const SRTextureInfo* info, SRTexture* texture, const SRSubresourceData* data);
void SRGFXVulkan_CreateSampler(SRGFXDevice* device, SRSamplerInfo info, SRSampler* sampler);

void SRGFXVulkan_DestroySwapchain(SRGFXDevice* device, SRSwapchain* swapchain);
void SRGFXVulkan_DestroyPipeline(SRGFXDevice* device, SRPipeline* pipeline);
void SRGFXVulkan_DestroyResource(SRGFXDevice* device, SRResource* resource);

void SRGFXVulkan_BindPipeline(SRGFXDevice* device, const SRPipeline* pipeline, SRCmdList cmdList);
void SRGFXVulkan_BindViewport(SRGFXDevice* device, const SRViewport* viewport, SRCmdList cmdList);
void SRGFXVulkan_BindVertexBuffer(SRGFXDevice* device, const SRBuffer* buffer, SRCmdList cmdList);
void SRGFXVulkan_BindIndexBuffer(SRGFXDevice* device, const SRBuffer* buffer, SRCmdList cmdList);
void SRGFXVulkan_BindRootConstantBuffer(SRGFXDevice* device, const SRBuffer* buffer, SRCmdList cmdList);
void SRGFXVulkan_PushConstants(SRGFXDevice* device, const void* data, u32 size, SRCmdList cmdList);
void SRGFXVulkan_Barrier(SRGFXDevice* device, const SRBarrier* barriers, u32 numBarriers, SRCmdList cmdList);

void SRGFXVulkan_BeginFrame(SRGFXDevice* device, const SRSwapchain* swapchain);
SRCmdList SRGFXVulkan_BeginCommandList(SRGFXDevice* device, SRQueue queue);
void SRGFXVulkan_BeginRenderPassSwapchain(SRGFXDevice* device, const SRSwapchain* swapchain, SRCmdList cmdList);
void SRGFXVulkan_BeginRenderPass(SRGFXDevice* device, const SRPassInfo* passInfo, SRCmdList cmdList);
void SRGFXVulkan_EndRenderPassSwapchain(SRGFXDevice* device, const SRSwapchain* swapchain, SRCmdList cmdList);
void SRGFXVulkan_EndRenderPass(SRGFXDevice* device, SRCmdList cmdList);
void SRGFXVulkan_SubmitCommandLists(SRGFXDevice* device, const SRSwapchain* swapchain);

void SRGFXVulkan_Draw(SRGFXDevice* device, u32 vtxCount, u32 startVtx, SRCmdList cmdList);
void SRGFXVulkan_DrawIndexed(SRGFXDevice* device, u32 idxCount, u32 startIdx, u32 baseVtx, SRCmdList cmdList);
void SRGFXVulkan_DrawInstanced(SRGFXDevice* device, u32 vtx_count, u32 inst_count, u32 start_vtx, uint32_t start_inst, SRCmdList cmd_list);
void SRGFXVulkan_DispatchMesh(SRGFXDevice* device, u32 x, u32 y, u32 z, SRCmdList cmdList);

SRDescriptorIndex SRGFXVulkan_GetDescriptorIndexSRV(SRGFXDevice* device, SRResource resource);
SRShaderCompileTarget SRGFXVulkan_GetShaderCompileTarget(SRGFXDevice* device);
void SRGFXVulkan_WaitForGPU(SRGFXDevice* device);
void SRGFXVulkan_FlushInitialUploads(SRGFXDevice* device);
