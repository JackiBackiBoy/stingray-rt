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
	void initialize(GFXDevice& gfxDevice);
	void destroy();

	std::unique_ptr<Model> create_plane(float width, float depth);
	std::unique_ptr<Model> create_sphere(float radius, int latitudeSplits, int longitudeSplits);

	void load_from_file(Asset& outAsset, const std::string& path);
}