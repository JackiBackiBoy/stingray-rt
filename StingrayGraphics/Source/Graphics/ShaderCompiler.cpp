#include "ShaderCompiler.h"
#include "Core/Logger.h"
#include "Utilities/TextUtilities.h"

#include <Unknwn.h>
#include <dxcapi.h>
#include <assert.h>

#include <stdlib.h>
#include <string.h>

#define HR(hr) do { HRESULT _hr = (hr); assert(SUCCEEDED(_hr)); } while (0)

struct SRShaderCompiler {
	SRGFXBackend backend;
	SRArena* arena;
	IDxcUtils* dxc_utils;
	IDxcCompiler3* dxc_compiler;
	IDxcIncludeHandler* dxc_include_handler;
};

SRShaderCompiler* SRShaderCompiler_Create(SRArena* arena, SRGFXBackend backend) {
	SRShaderCompiler* compiler = SRArena_PushStructZero(arena, SRShaderCompiler);
	compiler->backend = backend;
	compiler->arena = arena;

	HMODULE dxc_dll = LoadLibrary(L"dxcompiler.dll");
	assert(dxc_dll);

	DxcCreateInstanceProc DxcCreateInstance = (DxcCreateInstanceProc)GetProcAddress(dxc_dll, "DxcCreateInstance");
	HR(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&compiler->dxc_utils)));
	HR(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler->dxc_compiler)));
	HR(compiler->dxc_utils->CreateDefaultIncludeHandler(&compiler->dxc_include_handler));

	return compiler;
}

void SRShaderCompiler_Destroy(SRShaderCompiler* compiler) {
	compiler->dxc_include_handler->Release();
	compiler->dxc_compiler->Release();
	compiler->dxc_utils->Release();
}

void SRShaderCompiler_CompileFromFile(SRShaderCompiler* compiler, const char* path, SRShaderCompileInfo info, SRShader* shader) {
	u64 path_len = strlen(path);
	const char* last_forward_slash = strrchr(path, '/');
	const char* last_backward_slash = strrchr(path, '\\');
	const char* last_slash = nullptr;

	if ((uintptr_t)last_forward_slash > (uintptr_t)last_backward_slash) {
		last_slash = last_forward_slash;
	}
	else {
		last_slash = last_backward_slash;
	}

	u64 dir_size = (uintptr_t)(last_slash - path);
	char* dir_str = (char*)malloc(dir_size + 1);
	memcpy(dir_str, path, dir_size);
	dir_str[dir_size] = '\0';

	SRWideTemp wstr_path = SRWideTemp(path);
	SRWideTemp wstr_dir = SRWideTemp(dir_str);
	SRWideTemp wstr_entry_point = SRWideTemp(info.entry_point);

	IDxcBlobEncoding* blob_src = nullptr;
	HR(compiler->dxc_utils->LoadFile(wstr_path, nullptr, &blob_src));

	const WCHAR* profile;
	switch (info.stage) {
	case SRShaderStage::Vertex:
		profile = L"vs_6_6";
		break;
	case SRShaderStage::Pixel:
		profile = L"ps_6_6";
		break;
	case SRShaderStage::Compute:
		profile = L"cs_6_6";
		break;
	case SRShaderStage::Mesh:
		profile = L"ms_6_6";
		break;
	default:
		profile = L"";
		break;
	}

	const WCHAR* args[32];
	UINT32 arg_count = 0;
	args[arg_count++] = L"-HV";
	args[arg_count++] = L"2021";
	args[arg_count++] = L"-I";
	args[arg_count++] = wstr_dir;
	args[arg_count++] = L"-E";
	args[arg_count++] = wstr_entry_point;
	args[arg_count++] = L"-T";
	args[arg_count++] = profile;
	args[arg_count++] = DXC_ARG_PACK_MATRIX_COLUMN_MAJOR;
	args[arg_count++] = DXC_ARG_WARNINGS_ARE_ERRORS;
	args[arg_count++] = DXC_ARG_ALL_RESOURCES_BOUND;

	if (compiler->backend == SRGFXBackend::Vulkan) {
		args[arg_count++] = L"-spirv";
		args[arg_count++] = L"-fspv-target-env=vulkan1.3";
		args[arg_count++] = L"-fvk-use-dx-layout";
		args[arg_count++] = L"-fvk-bind-resource-heap";
		args[arg_count++] = L"0"; // binding
		args[arg_count++] = L"0"; // set
		args[arg_count++] = L"-fvk-bind-sampler-heap";
		args[arg_count++] = L"1"; // binding
		args[arg_count++] = L"0"; // set
		args[arg_count++] = L"-DSR_VULKAN";
	}

#if defined(_DEBUG)
	args[arg_count++] = DXC_ARG_DEBUG;
	args[arg_count++] = DXC_ARG_SKIP_OPTIMIZATIONS;
#else
	args[arg_count++] = DXC_ARG_OPTIMIZATION_LEVEL3;
#endif

	DxcBuffer src_buffer = {
		.Ptr = blob_src->GetBufferPointer(),
		.Size = blob_src->GetBufferSize(),
		.Encoding = 0
	};
	IDxcResult* compiled_shader = nullptr;
	HRESULT hr = compiler->dxc_compiler->Compile(
		&src_buffer,
		args,
		arg_count,
		compiler->dxc_include_handler,
		IID_PPV_ARGS(&compiled_shader)
	);

	if (FAILED(hr)) {
		WCHAR outStr[MAX_PATH] = {};
		wsprintfW(outStr, L"Failed to compile shader at: %ls", wstr_path.buffer.data());
		OutputDebugStringW(outStr);
	}

	IDxcBlobUtf8* blob_errors = nullptr;
	HR(compiled_shader->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&blob_errors), nullptr));

	if (blob_errors && blob_errors->GetStringLength() > 0) {
		OutputDebugStringA(blob_errors->GetStringPointer());
	}

	IDxcBlob* blob_compiled_shader = nullptr;
	HR(compiled_shader->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&blob_compiled_shader), nullptr));

	u64 buffer_size = blob_compiled_shader->GetBufferSize();
	shader->data = SRArena_PushBytes(compiler->arena, buffer_size);
	shader->size = buffer_size;
	shader->entry_point = info.entry_point;

	memcpy(shader->data, blob_compiled_shader->GetBufferPointer(), buffer_size);

	blob_compiled_shader->Release();
	blob_errors->Release();
	compiled_shader->Release();
	blob_src->Release();
	free(dir_str);
}
