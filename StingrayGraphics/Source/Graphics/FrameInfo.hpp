#pragma once

#include "Data/Camera.hpp"

struct SRFrameInfo {
	SRCamera* camera;
	SRBuffer* perFrameBuffer;
	float dt;
	int width;
	int height;
};