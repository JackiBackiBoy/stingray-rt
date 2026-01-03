#pragma once

#include "Data/ArenaAllocator.h"
#include "Graphics/GraphicsTypes.h"

struct SRShaderCompiler;
struct SRShaderCompileInfo {
	SRShaderStage stage;
	const char* entry_point;
};

SRShaderCompiler* SRShaderCompiler_Create(SRArena* arena, SRGFXBackend backend);
void              SRShaderCompiler_Destroy(SRShaderCompiler* compiler);
void              SRShaderCompiler_CompileFromFile(SRShaderCompiler* compiler, const char* path, SRShaderCompileInfo info, SRShader* shader);