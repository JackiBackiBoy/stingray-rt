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
void SRGFX_CreateSwapchain(SRGFXDevice* device, const SRWindow* window, const SRSwapchainInfo* info, SRSwapchain* swapchain) {
	device->vtbl->create_swapchain(device, info, swapchain);
}
void SRGFX_CreatePipeline(SRGFXDevice* device, const SRPipelineInfo* info, SRPipeline* pipeline) {
	device->vtbl->create_pipeline(device, info, pipeline);
}
void SRGFX_CreateBuffer(SRGFXDevice* device, const SRBufferInfo* info, SRBuffer* buffer, const void* data) {
	device->vtbl->create_buffer(device, info, buffer, data);
}
void SRGFX_CreateTexture(SRGFXDevice* device, const SRTextureInfo* info, SRTexture* texture, const SRSubresourceData* data) {
	device->vtbl->create_texture(device, info, texture, data);
}
void SRGFX_CreateSampler(SRGFXDevice* device, const SRSamplerInfo* info, SRSampler* sampler) {
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
void SRGFX_BindPipeline(SRGFXDevice* device, const SRPipeline* pipeline, const SRCmdList* cmdList) {
	device->vtbl->bind_pipeline(device, pipeline, cmdList);
}
void SRGFX_BindViewport(SRGFXDevice* device, const SRViewport* viewport, const SRCmdList* cmdList) {
	device->vtbl->bind_viewport(device, viewport, cmdList);
}
void SRGFX_BindVertexBuffer(SRGFXDevice* device, const SRBuffer* buffer, const SRCmdList* cmdList) {
	device->vtbl->bind_vertex_buffer(device, buffer, cmdList);
}
void SRGFX_BindIndexBuffer(SRGFXDevice* device, const SRBuffer* buffer, const SRCmdList* cmdList) {
	device->vtbl->bind_index_buffer(device, buffer, cmdList);
}
void SRGFX_BindRootConstantBuffer(SRGFXDevice* device, const SRBuffer* buffer, const SRCmdList* cmdList) {
	device->vtbl->bind_root_constant_buffer(device, buffer, cmdList);
}
void SRGFX_PushConstants(SRGFXDevice* device, const void* data, u32 size, const SRCmdList* cmdList) {
	device->vtbl->push_constants(device, data, size, cmdList);
}
void SRGFX_Barrier(SRGFXDevice* device, const SRBarrier* barriers, u32 numBarriers, const SRCmdList* cmdList) {
	device->vtbl->barrier(device, barriers, numBarriers, cmdList);
}

// Command lists and render passes
void SRGFX_BeginFrame(SRGFXDevice* device, const SRSwapchain* swapchain) {
	device->vtbl->begin_frame(device, swapchain);
}
SRCmdList SRGFX_BeginCommandList(SRGFXDevice* device, SRQueue queue) {
	return device->vtbl->begin_command_list(device, queue);
}
void SRGFX_BeginRenderPassSwapchain(SRGFXDevice* device, const SRSwapchain* swapchain, const SRCmdList* cmdList) {
	device->vtbl->begin_render_pass_swapchain(device, swapchain, cmdList);
}
void SRGFX_BeginRenderPass(SRGFXDevice* device, const SRPassInfo* passInfo, const SRCmdList* cmdList) {
	device->vtbl->begin_render_pass(device, passInfo, cmdList);
}
void SRGFX_EndRenderPassSwapchain(SRGFXDevice* device, const SRSwapchain* swapchain, const SRCmdList* cmdList) {
	device->vtbl->end_render_pass_swapchain(device, swapchain, cmdList);
}
void SRGFX_EndRenderPass(SRGFXDevice* device, const SRCmdList* cmdList) {
	device->vtbl->end_render_pass(device, cmdList);
}
void SRGFX_SubmitCommandLists(SRGFXDevice* device, const SRSwapchain* swapchain) {
	device->vtbl->submit_command_lists(device, swapchain);
}

// Draw
void SRGFX_Draw(SRGFXDevice* device, u32 vtxCount, u32 startVtx, const SRCmdList* cmdList) {
	device->vtbl->draw(device, vtxCount, startVtx, cmdList);
}
void SRGFX_DrawIndexed(SRGFXDevice* device, u32 idxCount, u32 startIdx, u32 baseVtx, const SRCmdList* cmdList) {
	device->vtbl->draw_indexed(device, idxCount, startIdx, baseVtx, cmdList);
}

void SRGFX_DrawInstanced(SRGFXDevice* device, u32 vtx_count, u32 inst_count, u32 start_vtx, uint32_t start_inst, const SRCmdList* cmd_list) {
	device->vtbl->draw_instanced(device, vtx_count, inst_count, start_vtx, start_inst, cmd_list);
}

void SRGFX_DispatchMesh(SRGFXDevice* device, u32 x, u32 y, u32 z, const SRCmdList* cmdList) {
	device->vtbl->dispatch_mesh(device, x, y, z, cmdList);
}

// Misc
SRDescriptorIndex SRGFX_GetDescriptorIndexSRV(SRGFXDevice* device, const SRResource* resource) {
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
void SRGFX_SetupImGuiInitInfo(SRGFXDevice* device, SRFormat swapchainFormat) {
	device->vtbl->setup_imgui_init_info(device, swapchainFormat);
}
