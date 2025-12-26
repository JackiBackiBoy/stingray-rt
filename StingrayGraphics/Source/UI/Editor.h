#pragma once

#include "Core/Window.h"
#include "Graphics/GraphicsDevice.h"
#include "Graphics/RenderGraph.h"

class SREditor {
public:
	SREditor(SRWindow& window, SRGFXDevice& gfxDevice, SRGFXBackend api);
	~SREditor();

	void update(SRRenderGraph& renderGraph);
	void render(const SRCmdList& cmdList);

private:
	SRWindow& m_Window;
	SRGFXDevice& m_GfxDevice;
	SRGFXBackend m_API;
	void (*begin_render_func)() = nullptr;
	void (*end_render_func)(const SRCmdList& cmdList) = nullptr;
	void (*shutdown_func)() = nullptr;
};