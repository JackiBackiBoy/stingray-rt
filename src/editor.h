#pragma once

#include "graphics/gfx_device.h"

class Editor {
public:
	Editor(GFXDevice& gfxDevice);
	~Editor() {}

private:
	GFXDevice& m_GfxDevice;

};