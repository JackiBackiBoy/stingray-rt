#pragma once

#include "Data/Camera.hpp"
#include "Data/Scene.hpp"

struct SRFrameInfo {
	SRCamera* camera;
	SRBuffer* perFrameBuffer;
	SRScene* scene;
	float dt;
	int width;
	int height;
};
