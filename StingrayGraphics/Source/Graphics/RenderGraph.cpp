#include "RenderGraph.hpp"

#include <cassert>

// -------------------------------- Render Pass --------------------------------
SRRenderPass& SRRenderPass::add_color_input(const std::string& name, SRAccessFlag accessFlags, SRSubresourceRange subrange /*= SRSubresourceRange::All()*/) {
	SRRenderPassAttachment* attachment = m_RenderGraph.get_attachment(name);

	assert(subrange.baseMip < attachment->mipLevels);
	if (subrange.mipCount == ~0u)
		subrange.mipCount = attachment->mipLevels - subrange.baseMip;

	assert(subrange.baseMip + subrange.mipCount <= attachment->mipLevels);

	SRRenderPassAttachmentInput inputAttachment;
	inputAttachment.attachment = attachment;
	inputAttachment.accessFlags = accessFlags;
	inputAttachment.subresources = subrange;

	if (accessFlags & SRAccessFlag_Read) {
		attachment->readInPasses.push_back(m_Index);
		inputAttachment.targetState = SRResourceState::SHADER_RESOURCE;
	}
	if (accessFlags & SRAccessFlag_Write) {
		attachment->writtenInPasses.push_back(m_Index);
		inputAttachment.targetState = SRResourceState::UNORDERED_ACCESS;
	}

	m_InputAttachments.push_back(inputAttachment);
	return *this;
}

SRRenderPass& SRRenderPass::add_depth_input(const std::string& name) {
	SRRenderPassAttachment* attachment = m_RenderGraph.get_attachment(name);
	attachment->readInPasses.push_back(m_Index);

	m_InputAttachments.push_back({ attachment, SRResourceState::DEPTH_READ, SRAccessFlag_Read, { 0, 1, 0, 0 } });
	return *this;
}

SRRenderPass& SRRenderPass::add_color_output(const std::string& name, int width, int height, SRFormat format, int mipLevels /*= 1*/, SRSizeClass sizeClass /*= SRSizeClass::SWAPCHAIN_RELATIVE*/) {
	// TODO: Base mip?
	SRRenderPassAttachment* attachment = m_RenderGraph.get_attachment(name);
	attachment->width = static_cast<uint32_t>(width);
	attachment->height = static_cast<uint32_t>(height);
	attachment->mipLevels = static_cast<uint32_t>(mipLevels);
	attachment->format = format;
	attachment->sizeClass = sizeClass;
	attachment->type = SRRenderPassAttachment::Type::RENDER_TARGET;
	attachment->writtenInPasses.push_back(m_Index);

	attachment->subresourceStates.resize(static_cast<size_t>(mipLevels));
	for (int i = 0; i < mipLevels; ++i) {
		SRRenderPassAttachmentSubresource& subresourceState = attachment->subresourceStates[i];
		//subresourceState.state = SRResourceState::RENDER_TARGET;
		subresourceState.lastBarrierAccess = SRBarrierAccess::None;
	}

	m_OutputAttachments.push_back(attachment);
	return *this;
}

SRRenderPass& SRRenderPass::add_depth_output(const std::string& name, int width, int height, SRFormat format, float clearValue /*= 0.0f*/, SRSizeClass sizeClass /*= SRSizeClass::SWAPCHAIN_RELATIVE*/) {
	SRRenderPassAttachment* attachment = m_RenderGraph.get_attachment(name);
	attachment->width = static_cast<uint32_t>(width);
	attachment->height = static_cast<uint32_t>(height);
	attachment->mipLevels = 1;
	attachment->format = format;
	attachment->sizeClass = sizeClass;
	attachment->subresourceStates.push_back({ SRResourceState::UNDEFINED });
	attachment->type = SRRenderPassAttachment::Type::DEPTH_STENCIL;
	attachment->writtenInPasses.push_back(m_Index);
	attachment->depthClearValue = clearValue;

	m_OutputAttachments.push_back(attachment);
	return *this;
}

SRRenderPass& SRRenderPass::add_rw_texture_output(const std::string& name, int width, int height, SRFormat format, int mipLevels /*= 1*/, SRSizeClass sizeClass /*= SRSizeClass::SWAPCHAIN_RELATIVE*/) {
	SRRenderPassAttachment* attachment = m_RenderGraph.get_attachment(name);
	attachment->width = static_cast<uint32_t>(width);
	attachment->height = static_cast<uint32_t>(height);
	attachment->mipLevels = mipLevels;
	attachment->format = format;
	attachment->sizeClass = sizeClass;
	attachment->type = SRRenderPassAttachment::Type::RW_TEXTURE;
	attachment->writtenInPasses.push_back(m_Index);

	attachment->subresourceStates.resize(static_cast<size_t>(mipLevels));
	for (int i = 0; i < mipLevels; ++i) {
		SRRenderPassAttachmentSubresource& subresourceState = attachment->subresourceStates[i];
		//subresourceState.state = SRResourceState::UNORDERED_ACCESS;
		subresourceState.lastBarrierAccess = SRBarrierAccess::None;
	}

	m_OutputAttachments.push_back(attachment);
	return *this;
}

SRRenderPass& SRRenderPass::set_execute_callback(std::function<void(SRRenderPass& self, SRGraphicsDevice& gfxDevice, const SRCmdList& cmdList, const SRFrameInfo& frameInfo)> callback) {
	m_ExecuteCallback = callback;
	return *this;
}

void SRRenderPass::execute(SRGraphicsDevice& gfxDevice, const SRCmdList& cmdList, const SRFrameInfo& frameInfo) {
	if (m_ExecuteCallback) {
		m_ExecuteCallback(*this, gfxDevice, cmdList, frameInfo);
	}
}

SRRenderPassAttachment* SRRenderPass::get_attachment(const std::string& name) {
	// TODO: Update this function to just operate on its inputs/outputs
	return m_RenderGraph.get_attachment(name);
}

const std::vector<SRRenderPassAttachment*>& SRRenderPass::get_output_attachments() const {
	return m_OutputAttachments;
}

const std::vector<SRRenderPassAttachmentInput>& SRRenderPass::get_input_attachments() const {
	return m_InputAttachments;
}

// -------------------------------- Render Graph -------------------------------

SRRenderPass& SRRenderGraph::add_render_pass(const std::string& name, SRPassType type) {
	const auto search = m_PassIndexLUT.find(name);
	assert(search == m_PassIndexLUT.end());

	const size_t passIndex = m_RenderPasses.size();
	auto pass = std::make_unique<SRRenderPass>(*this, static_cast<uint32_t>(passIndex), name, type);
	SRRenderPass& passRef = *pass;

	m_RenderPasses.push_back(std::move(pass));
	m_PassIndexLUT.insert({ name, passIndex });

	return passRef;
}

PrefabPass& SRRenderGraph::add_prefab_pass(const std::string& name) {
	const auto search = m_PrefabPassIndexLUT.find(name);
	assert(search == m_PrefabPassIndexLUT.end());

	const size_t passIndex = m_PrefabPasses.size();
	auto pass = std::make_unique<PrefabPass>(name);
	PrefabPass& passRef = *pass;

	m_PrefabPasses.push_back(std::move(pass));
	m_PrefabPassIndexLUT.insert({ name, passIndex });

	return passRef;
}

void SRRenderGraph::build(SRGraphicsDevice& gfxDevice) {
	// NOTE: This render graph performs NO pass re-ordering of any kind and
	// does not currently deal with aliasing or transient resources.
	// This might change in the future, but for now the purpose of this
	// function is only to create resources for the passes in the order that
	// they were declared in.

	for (size_t i = 0; i < m_RenderPasses.size(); ++i) {
		const auto& outputs = m_RenderPasses[i]->get_output_attachments();

		for (SRRenderPassAttachment* output : outputs) {
			SRTextureInfo textureInfo = {
				.width = output->width,
				.height = output->height,
				.mipLevels = output->mipLevels,
				.format = output->format,
				.usage = SRUsage::DEFAULT,
				.bindFlags = SRBindFlag::None
			};

			if (!output->readInPasses.empty() && output->type != SRRenderPassAttachment::Type::DEPTH_STENCIL) {
				textureInfo.bindFlags |= SRBindFlag::ShaderResource;
			}

			bool writtenByCompute = false;
			for (uint32_t passIndex : output->writtenInPasses) {
				if (m_RenderPasses[passIndex]->get_type() == SRPassType::COMPUTE) {
					writtenByCompute = true;
					break;
				}
			}

			if (writtenByCompute) {
				textureInfo.bindFlags |= SRBindFlag::UnorderedAccess;
			}

			switch (output->type) {
			case SRRenderPassAttachment::Type::RENDER_TARGET:
				textureInfo.bindFlags |= SRBindFlag::RenderTarget;
				break;
			case SRRenderPassAttachment::Type::DEPTH_STENCIL:
				textureInfo.bindFlags |= SRBindFlag::DepthStencil;
				break;
			default:
				break;
			}

			gfxDevice.create_texture(textureInfo, output->texture, nullptr);
		}
	}
}

void SRRenderGraph::execute(SRGraphicsDevice& gfxDevice, const SRSwapchain& swapchain, const SRCmdList& cmdList, const SRFrameInfo& frameInfo) {
	bool encounteredFirstRootPass = false;

	for (size_t i = 0; i < m_RenderPasses.size(); ++i) {
		SRRenderPass* pass = m_RenderPasses[i].get();
		const auto& inputs = pass->get_input_attachments();
		const auto& outputs = pass->get_output_attachments();
		SRPassInfo passInfo = {};
		bool isFirstRootPass = false;

		if (!encounteredFirstRootPass && outputs.empty() && pass->get_type() == SRPassType::GRAPHICS) {
			encounteredFirstRootPass = true;
			isFirstRootPass = true;
			// NOTE: From here, we assume that all the passes that occur
			// after this point, will also be root passes.
		}

		// Output attachments
		std::vector<SRBarrier> barriers;
		for (SRRenderPassAttachment* output : outputs) {
			SRResourceState targetState = SRResourceState::UNDEFINED;
			SRBarrierAccess targetBarrierAccess = SRBarrierAccess::None;
			SRBarrierSync targetSyncPoint = SRBarrierSync::None;

			switch (output->type) {
			case SRRenderPassAttachment::Type::RENDER_TARGET:
			{
				auto& colorAttachment = passInfo.colorAttachments[passInfo.numColorAttachments++];
				colorAttachment.texture = &output->texture;
				colorAttachment.loadOp = SRLoadOp::Clear;
				colorAttachment.storeOp = SRStoreOp::Store;
				targetState = SRResourceState::RENDER_TARGET;
				targetBarrierAccess |= SRBarrierAccess::RenderTarget;
				targetSyncPoint = SRBarrierSync::RenderTarget;
			}
			break;
			case SRRenderPassAttachment::Type::DEPTH_STENCIL:
			{
				passInfo.depthAttachment.texture = &output->texture;
				passInfo.depthAttachment.loadOp = SRLoadOp::Clear;
				passInfo.depthAttachment.storeOp = SRStoreOp::Store;
				passInfo.depthAttachment.clearValue = output->depthClearValue;
				targetState = SRResourceState::DEPTH_WRITE;
				targetBarrierAccess |= SRBarrierAccess::DepthStencilWrite;
				targetSyncPoint = SRBarrierSync::DepthStencil;
			}
			break;
			case SRRenderPassAttachment::Type::RW_TEXTURE:
			{
				targetState = SRResourceState::UNORDERED_ACCESS;
				targetBarrierAccess |= SRBarrierAccess::UnorderedAccess;
				targetSyncPoint = SRBarrierSync::ComputeShader; // TODO: Perhaps not all of the time?
			}
			break;
			}

			for (uint32_t mip = 0; mip < output->subresourceStates.size(); ++mip) {
				SRRenderPassAttachmentSubresource& subresource = output->subresourceStates[mip];
				if (subresource.state != targetState) {
					// TODO: MAKE THESE BARRIERS WORK
					SRBarrier barrier = {
						.type = SRBarrierType::IMAGE,
						.image = {
							.texture = &output->texture,
							.stateBefore = subresource.state,
							.stateAfter = targetState,
							.accessBefore = subresource.lastBarrierAccess,
							.accessAfter = targetBarrierAccess,
							.syncBefore = subresource.lastBarrierStage,
							.syncAfter = targetSyncPoint 
						}
					};
					// TODO: Handle subresources better
					barriers.push_back(barrier);

					subresource.state = targetState;
				}

				subresource.lastBarrierAccess = targetBarrierAccess;
				subresource.lastBarrierStage = targetSyncPoint;
			}
		}

		// Input attachments
		for (const SRRenderPassAttachmentInput& input : inputs) {
			// NOTE: Some passes might use the depth pre-pass output as its
			// depth buffer input, thus it's important that we do not alter
			// the input depth buffer in any way. Hence why we use SRStoreOp::None.
			if (input.attachment->type == SRRenderPassAttachment::Type::DEPTH_STENCIL) {
				passInfo.depthAttachment.texture = &input.attachment->texture;
				passInfo.depthAttachment.loadOp = SRLoadOp::Load;
				passInfo.depthAttachment.storeOp = SRStoreOp::None; // TODO: Might break
			}

			for (uint32_t mip = input.subresources.baseMip; mip < input.subresources.baseMip + input.subresources.mipCount; ++mip) {
				SRRenderPassAttachmentSubresource& subresource = input.attachment->subresourceStates[mip];
				
				SRBarrier barrier = {
					.type = SRBarrierType::IMAGE,
					.image = {
						.texture = &input.attachment->texture,
						.stateBefore = subresource.state,
						.stateAfter = input.targetState,
						.accessBefore = subresource.lastBarrierAccess,
						.syncBefore = subresource.lastBarrierStage,
					}
				};

				// TODO: syncAfter (dstStageMask) depends entirely on what the current pass
				// will do. So for now, it might work for simple raster, but will likely have to be extended
				if (input.accessFlags & SRAccessFlag_Read) {
					if (input.attachment->type == SRRenderPassAttachment::Type::DEPTH_STENCIL) {
						barrier.image.accessAfter = SRBarrierAccess::DepthStencilRead;
						barrier.image.syncAfter = SRBarrierSync::DepthStencil;
					}
					else if (input.attachment->type == SRRenderPassAttachment::Type::RENDER_TARGET) {
						barrier.image.accessAfter = SRBarrierAccess::ShaderResource;
						barrier.image.syncAfter = SRBarrierSync::PixelShader;
					}
				}
				if (input.accessFlags & SRAccessFlag_Write) {
					if (input.attachment->type == SRRenderPassAttachment::Type::DEPTH_STENCIL) {
						// invalid
						assert(false);
					}
					else if (input.attachment->type == SRRenderPassAttachment::Type::RENDER_TARGET) {
						// invalid
						assert(false);
					}

					// TODO: Unordered access (RW texture) is the only applicable one, implement it
				}

				if (subresource.state != input.targetState) {
					subresource.state = input.targetState;
					barriers.push_back(barrier);
				}

				subresource.lastBarrierAccess = barrier.image.accessAfter;
				subresource.lastBarrierStage = barrier.image.syncAfter;
			}
		}

		// Execute resource barriers if any
		if (!barriers.empty()) {
			gfxDevice.barrier(
				barriers.data(),
				static_cast<uint32_t>(barriers.size()),
				cmdList
			);
		}

		// Swapchain passes (writes directly to swapchain)
		// NOTE: These  kinds of passes require special handling, the logic
		// we have right now is not perfect. However, the core idea behind
		// "swapchain passes" is that they are passes that write to the same
		// resource, namely the swapchain back buffer. Thus, it does not
		// make sense to Begin/End render passes if they all write to the
		// same resource. Hence we only Begin a pass if it's the first pass
		// that writes to swapchain, and only End a pass if it's the last
		// pass to write to the swapchain.
		if (encounteredFirstRootPass) {
			if (isFirstRootPass) {
				gfxDevice.begin_render_pass(swapchain, cmdList);
			}

			pass->execute(gfxDevice, cmdList, frameInfo);

			if (i == m_RenderPasses.size() - 1) {
				gfxDevice.end_render_pass(swapchain, cmdList);
			}
			continue;
		}

		// "Normal" render passes
		if (pass->get_type() == SRPassType::GRAPHICS) {
			gfxDevice.begin_render_pass(passInfo, cmdList);
		}

		pass->execute(gfxDevice, cmdList, frameInfo);
		if (pass->get_type() == SRPassType::GRAPHICS) {
			gfxDevice.end_render_pass(passInfo, cmdList);
		}
	}
}

void SRRenderGraph::notify_swapchain_resize(SRGraphicsDevice& gfxDevice, int newWidth, int newHeight) {
	for (auto& attachment : m_Attachments) {
		// TODO: Right now we assume that "swapchain relative" means that an
		// attachment will have the EXACT dimensions as the swapchain. But
		// we might want it to only scale with the swapchain instead.
		if (attachment->sizeClass == SRSizeClass::SWAPCHAIN_RELATIVE) {
			SRTextureInfo newTextureInfo = attachment->texture.info;
			newTextureInfo.width = newWidth;
			newTextureInfo.height = newHeight;

			//gfxDevice.CreateTexture(newTextureInfo, attachment->texture, nullptr);

			// Reset subresource states to the default state, depending on
			// the resource type.
			//for (SRResourceState& resourceState : attachment->subresourceStates) {
			//	switch (attachment->type) {
			//	case SRRenderPassAttachment::Type::RENDER_TARGET:
			//	{
			//		resourceState = SRResourceState::RENDER_TARGET;
			//	}
			//	break;
			//	case SRRenderPassAttachment::Type::DEPTH_STENCIL:
			//	{
			//		resourceState = SRResourceState::DEPTH_WRITE;
			//	}
			//	break;
			//	case SRRenderPassAttachment::Type::RW_TEXTURE:
			//	{
			//		resourceState = SRResourceState::UNORDERED_ACCESS;
			//	}
			//	break;
			//	}
			//}
		}
	}
}

SRRenderPassAttachment* SRRenderGraph::get_attachment(const std::string& name) {
	auto search = m_AttachmentIndexLUT.find(name);

	// Return existing attachment
	if (search != m_AttachmentIndexLUT.end()) {
		return m_Attachments[search->second].get();
	}

	m_AttachmentIndexLUT.insert({ name, m_AttachmentIndexLUT.size() });

	// Create new attachment if not present
	auto attachment = std::make_unique<SRRenderPassAttachment>();
	attachment->name = name;
	SRRenderPassAttachment* pAttachment = attachment.get();

	m_Attachments.push_back(std::move(attachment));

	return pAttachment;
}
