#pragma once

#include "Graphics/GraphicsTypes.h"

struct SRShaderCompileInfo {
	SRShaderStage stage = {};
	const char* entryPoint = "main";
};

class SRShaderCompiler {
public:
	SRShaderCompiler(SRShaderCompileTarget compile_target);
	~SRShaderCompiler();

	void compile_from_file(const char* path, const SRShaderCompileInfo& info, SRShader& shader);

private:
	struct Impl;
	Impl* m_Impl;
};
