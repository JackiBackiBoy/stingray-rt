#pragma once

#include "data/camera.h"

struct FrameInfo {
	Camera* camera = nullptr;
	float dt;
};