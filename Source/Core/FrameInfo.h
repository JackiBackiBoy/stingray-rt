#pragma once

#include "Data/Camera.h"

namespace SR {
	struct FrameInfo {
		Camera* camera = nullptr;
		float dt;
		int width;
		int height;
	};
}
