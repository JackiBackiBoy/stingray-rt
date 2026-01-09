#pragma once

#include "Core/Types.h"
#include "Data/Camera.h"
#include "Data/Scene.h"

struct SRFrameInfo {
	SRCamera* camera;
	SRBuffer* perFrameBuffer;
	SRScene* scene;
	f32 dt;
	u32 width;
	u32 height;
};
