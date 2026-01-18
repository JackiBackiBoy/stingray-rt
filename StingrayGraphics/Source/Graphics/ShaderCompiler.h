#pragma once

#include "Core/StringTypes.h"
#include "Data/ArenaAllocator.h"
#include "Graphics/GraphicsTypes.h"

struct SRShaderCompiler;
struct SRShaderCompileInfo {
	SRShaderStage stage;
	Str8 entry_point;
};

SRShaderCompiler* SRShaderCompiler_Create(SRArena* arena, SRGFXBackend backend);
void              SRShaderCompiler_Destroy(SRShaderCompiler* compiler);
void              SRShaderCompiler_CompileFromFile(SRShaderCompiler* compiler, Str8 path, SRShaderCompileInfo info, SRShader* shader);