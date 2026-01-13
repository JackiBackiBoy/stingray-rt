#include "GraphicsDevice.h"

#include "Graphics/DX12/GraphicsDevice_DX12.h"
#include "Graphics/Vulkan/GraphicsDevice_Vulkan.h"

void SRGFX_CreateDevice(SRWindow* window, SRGFXDevice* device, SRGFXBackend backend) {
	switch (backend) {
	case SRGFXBackend::DX12:
		SRGFXDX12_CreateDevice(window, device);
		break;
	case SRGFXBackend::Vulkan:
		SRGFXVulkan_CreateDevice(window, device);
		break;
	}
}

void SRGFX_DestroyDevice(SRGFXDevice* device) {
	device->vtbl->destroy_device(device);
}
u32 SRGFX_GetFrameIndex(SRGFXDevice* device) {
	return device->vtbl->get_frame_index(device);
}

// Resource creation
void SRGFX_CreateSwapchain(SRGFXDevice* device, SRSwapchainInfo info, SRSwapchain* swapchain) {
	device->vtbl->create_swapchain(device, info, swapchain);
}
void SRGFX_CreatePipeline(SRGFXDevice* device, const SRPipelineInfo* info, SRPipeline* pipeline) {
	device->vtbl->create_pipeline(device, info, pipeline);
}
void SRGFX_CreateBuffer(SRGFXDevice* device, SRBufferInfo info, SRBuffer* buffer, const void* data) {
	device->vtbl->create_buffer(device, info, buffer, data);
}
void SRGFX_CreateTexture(SRGFXDevice* device, SRTextureInfo info, SRTexture* texture, const SRSubresourceData* data) {
	device->vtbl->create_texture(device, info, texture, data);
}
void SRGFX_CreateSampler(SRGFXDevice* device, SRSamplerInfo info, SRSampler* sampler) {
	device->vtbl->create_sampler(device, info, sampler);
}

void SRGFX_DestroySwapchain(SRGFXDevice* device, SRSwapchain* swapchain) {
	device->vtbl->destroy_swapchain(device, swapchain);
}
void SRGFX_DestroyPipeline(SRGFXDevice* device, SRPipeline* pipeline) {
	device->vtbl->destroy_pipeline(device, pipeline);
}
void SRGFX_DestroyResource(SRGFXDevice* device, SRResource* resource) {
	device->vtbl->destroy_resource(device, resource);
}

// Binding
void SRGFX_BindPipeline(SRGFXDevice* device, const SRPipeline* pipeline, SRCmdList cmd_list) {
	device->vtbl->bind_pipeline(device, pipeline, cmd_list);
}
void SRGFX_BindViewport(SRGFXDevice* device, const SRViewport* viewport, SRCmdList cmd_list) {
	device->vtbl->bind_viewport(device, viewport, cmd_list);
}
void SRGFX_BindVertexBuffer(SRGFXDevice* device, const SRBuffer* buffer, SRCmdList cmd_list) {
	device->vtbl->bind_vertex_buffer(device, buffer, cmd_list);
}
void SRGFX_BindIndexBuffer(SRGFXDevice* device, const SRBuffer* buffer, SRCmdList cmd_list) {
	device->vtbl->bind_index_buffer(device, buffer, cmd_list);
}
void SRGFX_BindRootConstantBuffer(SRGFXDevice* device, const SRBuffer* buffer, SRCmdList cmd_list) {
	device->vtbl->bind_root_constant_buffer(device, buffer, cmd_list);
}
void SRGFX_PushConstants(SRGFXDevice* device, const void* data, u32 size, SRCmdList cmd_list) {
	device->vtbl->push_constants(device, data, size, cmd_list);
}
void SRGFX_Barrier(SRGFXDevice* device, const SRBarrier* barriers, u32 numBarriers, SRCmdList cmd_list) {
	device->vtbl->barrier(device, barriers, numBarriers, cmd_list);
}

// Command lists and render passes
void SRGFX_BeginFrame(SRGFXDevice* device, const SRSwapchain* swapchain) {
	device->vtbl->begin_frame(device, swapchain);
}
SRCmdList SRGFX_BeginCommandList(SRGFXDevice* device, SRQueue queue) {
	return device->vtbl->begin_command_list(device, queue);
}
void SRGFX_BeginRenderPassSwapchain(SRGFXDevice* device, const SRSwapchain* swapchain, SRCmdList cmd_list) {
	device->vtbl->begin_render_pass_swapchain(device, swapchain, cmd_list);
}
void SRGFX_BeginRenderPass(SRGFXDevice* device, const SRPassInfo* passInfo, SRCmdList cmd_list) {
	device->vtbl->begin_render_pass(device, passInfo, cmd_list);
}
void SRGFX_EndRenderPassSwapchain(SRGFXDevice* device, const SRSwapchain* swapchain, SRCmdList cmd_list) {
	device->vtbl->end_render_pass_swapchain(device, swapchain, cmd_list);
}
void SRGFX_EndRenderPass(SRGFXDevice* device, SRCmdList cmd_list) {
	device->vtbl->end_render_pass(device, cmd_list);
}
void SRGFX_SubmitCommandLists(SRGFXDevice* device, const SRSwapchain* swapchain) {
	device->vtbl->submit_command_lists(device, swapchain);
}

// Draw
void SRGFX_Draw(SRGFXDevice* device, u32 vtxCount, u32 startVtx, SRCmdList cmd_list) {
	device->vtbl->draw(device, vtxCount, startVtx, cmd_list);
}
void SRGFX_DrawIndexed(SRGFXDevice* device, u32 idxCount, u32 startIdx, u32 baseVtx, SRCmdList cmd_list) {
	device->vtbl->draw_indexed(device, idxCount, startIdx, baseVtx, cmd_list);
}

void SRGFX_DrawInstanced(SRGFXDevice* device, u32 vtx_count, u32 inst_count, u32 start_vtx, uint32_t start_inst, SRCmdList cmd_list) {
	device->vtbl->draw_instanced(device, vtx_count, inst_count, start_vtx, start_inst, cmd_list);
}

void SRGFX_DispatchMesh(SRGFXDevice* device, u32 x, u32 y, u32 z, SRCmdList cmd_list) {
	device->vtbl->dispatch_mesh(device, x, y, z, cmd_list);
}

// Misc
SRDescriptorIndex SRGFX_GetDescriptorIndexSRV(SRGFXDevice* device, SRResource resource) {
	return device->vtbl->get_descriptor_index_srv(device, resource);
}
SRShaderCompileTarget SRGFX_GetShaderCompileTarget(SRGFXDevice* device) {
	return device->vtbl->get_shader_compile_target(device);
}
void SRGFX_WaitForGPU(SRGFXDevice* device) {
	device->vtbl->wait_for_gpu(device);
}
void SRGFX_FlushInitialUploads(SRGFXDevice* device) {
	device->vtbl->flush_initial_uploads(device);
}
