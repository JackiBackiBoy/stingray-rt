#pragma once

#include "Graphics/GraphicsTypes.hpp"

struct SRShaderCompileInfo {
	SRShaderStage stage = {};
	const char* entryPoint = "main";
};

class SRShaderCompiler {
public:
	SRShaderCompiler(const SRShaderPlatformInfo& shaderPlatform);
	~SRShaderCompiler();

	void compile_from_file(const char* path, const SRShaderCompileInfo& info, SRShader& shader);

private:
	struct Impl;
	Impl* m_Impl;
};
