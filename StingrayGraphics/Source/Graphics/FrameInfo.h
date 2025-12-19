#pragma once

#include "Data/Camera.h"
#include "Data/Scene.h"

struct SRFrameInfo {
	SRCamera* camera;
	SRBuffer* perFrameBuffer;
	SRScene* scene;
	float dt;
	int width;
	int height;
};
