#pragma once

#include "Data/camera.h"

struct FrameInfo {
	Camera* camera = nullptr;
	float dt;
	int width;
	int height;
};