#pragma once

#include "Core/Window.hpp"
#include "Graphics/GraphicsDevice.hpp"
#include "Graphics/RenderGraph.hpp"

class SREditor {
public:
	SREditor(SRWindow& window, SRGraphicsDevice& gfxDevice, SRGraphicsAPI api);
	~SREditor();

	void update(SRRenderGraph& renderGraph);
	void render(const SRCmdList& cmdList);

private:
	SRWindow& m_Window;
	SRGraphicsDevice& m_GfxDevice;
	SRGraphicsAPI m_API;
	void (*begin_render_func)() = nullptr;
	void (*end_render_func)(const SRCmdList& cmdList) = nullptr;
	void (*shutdown_func)() = nullptr;
};