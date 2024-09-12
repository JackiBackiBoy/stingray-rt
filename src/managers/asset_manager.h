#pragma once

#include "../data/model.h"
#include "../graphics/gfx_device.h"

#include <memory>
#include <string>

struct Asset {
	std::shared_ptr<void> internalState = nullptr;

	const Model* get_model() const;
	const Texture* get_texture() const;
	// ...
};

namespace assetmanager {
	void initialize();
	void destroy();

	void load_from_file(Asset& outAsset, const std::string& path, GFXDevice& gfxDevice);
}