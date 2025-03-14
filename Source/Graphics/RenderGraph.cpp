#include "RenderGraph.h"

#include <cassert>

namespace SR {
	// Renderpass
	void RenderPass::add_input_attachment(const std::string& name) {
		RenderPassAttachment* attachment = m_Graph.get_attachment(name);
		attachment->readInPasses.push_back(m_Index);

		m_InputAttachments.push_back(attachment);
	}

	void RenderPass::add_output_attachment(const std::string& name, const AttachmentInfo& info) {
		RenderPassAttachment* attachment = m_Graph.get_attachment(name);
		attachment->info = info;
		attachment->writtenInPasses.push_back(m_Index);

		// Initial state
		switch (info.type) {
		case AttachmentType::RENDER_TARGET:
			attachment->currentState = ResourceState::RENDER_TARGET;
			break;
		case AttachmentType::DEPTH_STENCIL:
			attachment->currentState = ResourceState::DEPTH_WRITE;
			break;
		case AttachmentType::RW_TEXTURE:
			attachment->currentState = ResourceState::UNORDERED_ACCESS;
			break;
		}

		m_OutputAttachments.push_back(attachment);
	}

	void RenderPass::execute(GraphicsDevice& gfxDevice, const CommandList& cmdList, const FrameInfo& frameInfo) {
		if (m_ExecuteCallback) {
			PassExecuteInfo executeInfo = {
				.renderGraph = &m_Graph,
				.gfxDevice = &gfxDevice,
				.cmdList = &cmdList,
				.frameInfo = &frameInfo
			};

			m_ExecuteCallback(executeInfo);
		}
	}

	// Rendergraph
	RenderPass* RenderGraph::add_pass(const std::string& name) {
		auto search = m_PassIndexLUT.find(name);

		if (search != m_PassIndexLUT.end()) {
			return m_Passes[search->second].get();
		}

		const size_t passIndex = m_Passes.size();
		m_PassIndexLUT.insert({ name, passIndex });

		auto pass = std::make_unique<RenderPass>(*this, passIndex, name);
		RenderPass* pPass = pass.get();

		m_Passes.push_back(std::move(pass));

		return pPass;
	}

	RenderPassAttachment* RenderGraph::get_attachment(const std::string& name) {
		auto search = m_AttachmentIndexLUT.find(name);

		// Return existing attachment
		if (search != m_AttachmentIndexLUT.end()) {
			return m_Attachments[search->second].get();
		}

		m_AttachmentIndexLUT.insert({ name, m_AttachmentIndexLUT.size() });

		// Create new attachment if not present
		auto attachment = std::make_unique<RenderPassAttachment>();
		attachment->name = name;
		RenderPassAttachment* pAttachment = attachment.get();

		m_Attachments.push_back(std::move(attachment));

		return pAttachment;
	}

	void RenderGraph::build() {
		assert(!m_Passes.empty());
		recurse_build(static_cast<uint32_t>(m_Passes.size() - 1));
	}

	void RenderGraph::execute(const SwapChain& swapChain, const CommandList& cmdList, const FrameInfo& frameInfo) {
		bool clearTargets = true;
		bool encounteredFirstRootPass = false;
		bool begunSwapchainPass = false;

		for (size_t p = 0; p < m_Passes.size(); ++p) {
			bool isRootPass = false;

			RenderPass& pass = *m_Passes[p];
			auto& inputAttachments = pass.get_input_attachments();
			auto& outputAttachments = pass.get_output_attachments();
			PassInfo passInfo = {}; // general pass info to be sent to the device

			// Root pass must use the swapchain backbuffer
			if (encounteredFirstRootPass) {
				clearTargets = false;
			}

			if (p == m_Passes.size() - 1 || outputAttachments.empty()) {
				if (!encounteredFirstRootPass) {
					encounteredFirstRootPass = true;
				}

				isRootPass = true;
			}

			// Define outputs
			for (size_t a = 0; a < outputAttachments.size(); ++a) {
				RenderPassAttachment* attachment = outputAttachments[a];

				ResourceState targetState = {};

				switch (attachment->info.type) {
				case AttachmentType::RENDER_TARGET:
				{
					passInfo.colors[passInfo.numColorAttachments++] = &attachment->texture;
					targetState = ResourceState::RENDER_TARGET;
				}
				break;
				case AttachmentType::DEPTH_STENCIL:
				{
					passInfo.depth = &attachment->texture;
					targetState = ResourceState::DEPTH_WRITE;
				}
				break;
				case AttachmentType::RW_TEXTURE:
				{
					targetState = ResourceState::UNORDERED_ACCESS;
				}
				break;
				}

				if (attachment->currentState != targetState) {
					m_GfxDevice.barrier(
						GPUBarrier::imageBarrier(&attachment->texture, attachment->currentState, targetState),
						cmdList
					);
					attachment->currentState = targetState;
				}
			}

			// Pipeline barriers to await required input transition layouts
			for (size_t a = 0; a < inputAttachments.size(); ++a) {
				RenderPassAttachment* attachment = inputAttachments[a];
				ResourceState targetState = ResourceState::SHADER_RESOURCE;

				if (attachment->currentState != targetState) {
					m_GfxDevice.barrier(
						GPUBarrier::imageBarrier(&attachment->texture, attachment->currentState, targetState),
						cmdList
					);
					attachment->currentState = targetState;
				}
			}

			if (isRootPass) {
				// TODO: This could be problematic when it comes to pipeline barriers, but I don't know
				if (!begunSwapchainPass) {
					m_GfxDevice.begin_render_pass(swapChain, passInfo, cmdList, clearTargets);
					begunSwapchainPass = true;
				}

				const uint32_t width = swapChain.info.width;
				const uint32_t height = swapChain.info.height;
				const Viewport viewport = {
					.topLeftX = 0.0f,
					.topLeftY = 0.0f,
					.width = static_cast<float>(width),
					.height = static_cast<float>(height),
					.minDepth = 0.0f,
					.maxDepth = 1.0f
				};

				m_GfxDevice.bind_viewport(viewport, cmdList); // TODO: Might inadvertently flip the viewport all the time

				pass.execute(m_GfxDevice, cmdList, frameInfo);

				if (p == m_Passes.size() - 1) {
					m_GfxDevice.end_render_pass(swapChain, cmdList);
				}
			}
			else {
				if (outputAttachments[0]->info.type != AttachmentType::RW_TEXTURE) {
					m_GfxDevice.begin_render_pass(passInfo, cmdList);
				}

				const uint32_t width = outputAttachments[0]->info.width;
				const uint32_t height = outputAttachments[0]->info.height;
				const Viewport viewport = {
					.topLeftX = 0.0f,
					.topLeftY = 0.0f,
					.width = static_cast<float>(width),
					.height = static_cast<float>(height),
					.minDepth = 0.0f,
					.maxDepth = 1.0f
				};

				m_GfxDevice.bind_viewport(viewport, cmdList);

				pass.execute(m_GfxDevice, cmdList, frameInfo);

				if (outputAttachments[0]->info.type != AttachmentType::RW_TEXTURE) {
					m_GfxDevice.end_render_pass(cmdList);
				}
			}
		}
	}

	void RenderGraph::recurse_build(uint32_t index) {
		auto& pass = m_Passes[index];
		auto& inputs = pass->get_input_attachments();
		auto& outputs = pass->get_output_attachments();

		// Build all outputs
		for (size_t i = 0; i < outputs.size(); ++i) {
			RenderPassAttachment* attachment = outputs[i];

			TextureInfo textureInfo = {
				.width = attachment->info.width,
				.height = attachment->info.height,
				.depth = 1, // TODO: For now we do not support 3D textures,
				.arraySize = 1,
				.mipLevels = 1,
				.sampleCount = 1,
				.format = attachment->info.format,
				.usage = Usage::DEFAULT,
				.bindFlags = attachment->readInPasses.empty() ? BindFlag::NONE : BindFlag::SHADER_RESOURCE
			};

			switch (attachment->info.type) {
			case AttachmentType::RENDER_TARGET:
				textureInfo.bindFlags |= BindFlag::RENDER_TARGET;
				break;
			case AttachmentType::DEPTH_STENCIL:
				textureInfo.bindFlags |= BindFlag::DEPTH_STENCIL;
				break;
			case AttachmentType::RW_TEXTURE:
				textureInfo.bindFlags |= BindFlag::UNORDERED_ACCESS;
				break;
			}

			m_GfxDevice.create_texture(textureInfo, attachment->texture, nullptr);
		}

		for (size_t i = 0; i < inputs.size(); ++i) {
			const RenderPassAttachment* attachment = inputs[i];

			if (attachment->texture.internalState != nullptr) {
				continue;
			}

			for (size_t w = 0; w < attachment->writtenInPasses.size(); ++w) {
				recurse_build(attachment->writtenInPasses[w]);
			}
		}

		if (inputs.empty() && index > 0) {
			recurse_build(index - 1);
		}
	}
}
