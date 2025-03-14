#pragma once

#include "Graphics/GraphicsDevice.h"
#include "Graphics/RenderGraph.h"

namespace SR {
	class FullscreenTriPass {
	public:
		FullscreenTriPass(GraphicsDevice& gfxDevice);
		~FullscreenTriPass() {}

		void execute(PassExecuteInfo& executeInfo);
	private:
		struct PushConstant {
			uint32_t texIndex = 0;
		} m_PushConstant = {};

		GraphicsDevice& m_GfxDevice;
		Shader m_VertexShader = {};
		Shader m_PixelShader = {};
		Pipeline m_Pipeline = {};
	};
}
