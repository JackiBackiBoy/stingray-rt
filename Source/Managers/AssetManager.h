#pragma once

#include "Data/Font.h"
#include "Data/Model.h"
#include "Graphics/GraphicsDevice.h"
#include "Managers/MaterialManager.h"

#include <memory>
#include <string>

struct Asset {
	std::shared_ptr<void> internalState = nullptr;

	const Model* get_model() const;
	const Texture* get_texture() const;
	const Font* get_font() const;
	// ...
};

namespace assetmanager {
	void initialize(GFXDevice& gfxDevice, MaterialManager& materialManager);
	void destroy();

	std::unique_ptr<Model> create_plane(float width, float depth);
	std::unique_ptr<Model> create_sphere(float radius, int latitudeSplits, int longitudeSplits, const Material* material = nullptr);

	void load_from_file(Asset& outAsset, const std::string& path);
	Font* load_font_from_file(const std::string& path, int ptSize);
}