#include "ShaderCompiler.h"
#include "Core/Logger.h"
#include "Utilities/TextUtilities.h"

#include <Unknwn.h>
#include <dxcapi.h>
#include <assert.h>

#include <stdexcept>
#include <string>

#define HR(hr) do { HRESULT _hr = (hr); assert(SUCCEEDED(_hr)); } while (0)

struct SRShaderCompiler::Impl {
	Impl(const SRShaderPlatformInfo& shaderPlatform);
	~Impl();

	void compile_from_file(const char* path, const SRShaderCompileInfo& info, SRShader& shader);

	SRShaderPlatformInfo m_ShaderPlatformInfo;
	IDxcUtils* m_DXCUtils;
	IDxcCompiler3* m_DXCCompiler;
	IDxcIncludeHandler* m_DXCIncludeHandler;
};

SRShaderCompiler::Impl::Impl(const SRShaderPlatformInfo& shaderPlatform) : m_ShaderPlatformInfo(shaderPlatform) {
	HMODULE dxcDLL = LoadLibrary(L"dxcompiler.dll");
	assert(dxcDLL);

	DxcCreateInstanceProc DxcCreateInstance = (DxcCreateInstanceProc)GetProcAddress(dxcDLL, "DxcCreateInstance");
	HR(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&m_DXCUtils)));
	HR(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&m_DXCCompiler)));
	HR(m_DXCUtils->CreateDefaultIncludeHandler(&m_DXCIncludeHandler));
}

SRShaderCompiler::Impl::~Impl() {
	m_DXCUtils->Release();
	m_DXCCompiler->Release();
	m_DXCIncludeHandler->Release();
}

void SRShaderCompiler::Impl::compile_from_file(const char* path, const SRShaderCompileInfo& info, SRShader& shader) {
	std::string pathStr = path;
	std::string fileName = pathStr.substr(pathStr.find_last_of("/\\") + 1);
	std::string directory = pathStr.substr(0, pathStr.find_last_of("/\\"));

	SRWideTemp wPathStr = SRWideTemp(path);
	SRWideTemp wDirStr = SRWideTemp(directory.c_str());
	SRWideTemp wEntryPointStr = SRWideTemp(info.entryPoint);

	IDxcBlobEncoding* sourceBlob;
	HR(m_DXCUtils->LoadFile(wPathStr, nullptr, &sourceBlob));

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
	UINT32 argCount = 0;
	args[argCount++] = L"-HV";
	args[argCount++] = L"2021";
	args[argCount++] = L"-I";
	args[argCount++] = wDirStr;
	args[argCount++] = L"-E";
	args[argCount++] = wEntryPointStr;
	args[argCount++] = L"-T";
	args[argCount++] = profile;
	args[argCount++] = DXC_ARG_PACK_MATRIX_COLUMN_MAJOR;
	args[argCount++] = DXC_ARG_WARNINGS_ARE_ERRORS;
	args[argCount++] = DXC_ARG_ALL_RESOURCES_BOUND;

	if (m_ShaderPlatformInfo.target == SRShaderCompileTarget::SPIRV) {
		args[argCount++] = L"-spirv";
		args[argCount++] = L"-fspv-target-env=vulkan1.3";
		args[argCount++] = L"-fvk-use-dx-layout";
		args[argCount++] = L"-fvk-bind-resource-heap";
		args[argCount++] = L"0"; // binding
		args[argCount++] = L"0"; // set
		args[argCount++] = L"-fvk-bind-sampler-heap";
		args[argCount++] = L"1"; // binding
		args[argCount++] = L"0"; // set
		args[argCount++] = L"-DSR_VULKAN";
	}

	#if defined(_DEBUG)
		args[argCount++] = DXC_ARG_DEBUG;
		args[argCount++] = DXC_ARG_SKIP_OPTIMIZATIONS;
	#else
		args[argCount++] = DXC_ARG_OPTIMIZATION_LEVEL3;
	#endif

	DxcBuffer srcBuffer = {
		.Ptr = sourceBlob->GetBufferPointer(),
		.Size = sourceBlob->GetBufferSize(),
		.Encoding = 0
	};
	IDxcResult* compiledShaderBuffer = nullptr;
	HRESULT hr = m_DXCCompiler->Compile(
		&srcBuffer,
		args,
		argCount,
		m_DXCIncludeHandler,
		IID_PPV_ARGS(&compiledShaderBuffer)
	);

	if (FAILED(hr)) {
		WCHAR outStr[MAX_PATH] = {};
		wsprintfW(outStr, L"Failed to compile shader at: %ls", wPathStr.buffer.data());
		OutputDebugStringW(outStr);	
	}

	IDxcBlobUtf8* errors = nullptr;
	HR(compiledShaderBuffer->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr));

	if (errors && errors->GetStringLength() > 0) {
		OutputDebugStringA(errors->GetStringPointer());
	}

	IDxcBlob* compiledShaderBlob = nullptr;
	HR(compiledShaderBuffer->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&compiledShaderBlob), nullptr));

	shader.byteCode.resize(compiledShaderBlob->GetBufferSize());
	memcpy(shader.byteCode.data(), compiledShaderBlob->GetBufferPointer(), compiledShaderBlob->GetBufferSize());
	shader.entryPoint = info.entryPoint;

	compiledShaderBlob->Release();
	errors->Release();
	compiledShaderBuffer->Release();
	sourceBlob->Release();
}

SRShaderCompiler::SRShaderCompiler(const SRShaderPlatformInfo& shaderPlatformInfo) {
	m_Impl = new Impl(shaderPlatformInfo);
}

SRShaderCompiler::~SRShaderCompiler() {
	delete m_Impl;
	m_Impl = nullptr;
}

void SRShaderCompiler::compile_from_file(const char* path, const SRShaderCompileInfo& info, SRShader& shader) {
	m_Impl->compile_from_file(path, info, shader);
}
