#include "ShaderCompiler.hpp"
#include "Core/Logger.hpp"

#include <slang.h>
#include <slang-com-ptr.h>
#include <slang-com-helper.h>

#include <fstream>
#include <stdexcept>
#include <string>

#define SR_SLANG_CHECK(expr, msg)                                              \
	do {                                                                       \
		SlangResult res = (expr);                                              \
		if (SLANG_FAILED(res)) {                                               \
			SRLOG_ERROR_CAT("SLANG", "%s failed. Error code: %d", msg, res);   \
			throw std::runtime_error("Slang error: " msg);                     \
		}                                                                      \
	} while (0)

inline constexpr SlangCompileTarget to_slang_target(SRShaderCompileTarget target) {
	switch (target) {
	case SRShaderCompileTarget::GLSL:
		return SLANG_GLSL;
	case SRShaderCompileTarget::HLSL:
		return SLANG_GLSL;
	case SRShaderCompileTarget::SPIRV:
		return SLANG_SPIRV;
	case SRShaderCompileTarget::DXIL:
		return SLANG_DXIL;
	default:
		return SLANG_TARGET_UNKNOWN;
	}
}

inline constexpr SlangStage to_slang_stage(SRShaderStage stage) {
	switch (stage) {
	case SRShaderStage::Vertex:
		return SLANG_STAGE_VERTEX;
	case SRShaderStage::Pixel:
		return SLANG_STAGE_FRAGMENT;
	case SRShaderStage::Compute:
		return SLANG_STAGE_COMPUTE;
	default:
		return SLANG_STAGE_NONE;
	}
}

struct SRShaderCompiler::Impl {
	Impl(const SRShaderPlatformInfo& shaderPlatform);
	~Impl();

	void compile_from_file(const char* path, const SRShaderCompileInfo& info, SRShader& shader);

	SRShaderPlatformInfo m_ShaderPlatformInfo;
	slang::IGlobalSession* m_GlobalSession = nullptr;
};

SRShaderCompiler::Impl::Impl(const SRShaderPlatformInfo& shaderPlatform) : m_ShaderPlatformInfo(shaderPlatform) {
	SlangGlobalSessionDesc globalSessionDesc = {
		.apiVersion = SLANG_API_VERSION,
		.minLanguageVersion = SLANG_LANGUAGE_VERSION_2025,
	};

	SR_SLANG_CHECK(slang::createGlobalSession(&globalSessionDesc, &m_GlobalSession), "Global session creation");
}

SRShaderCompiler::Impl::~Impl() {
	m_GlobalSession->release();
	m_GlobalSession = nullptr;
}

void SRShaderCompiler::Impl::compile_from_file(const char* path, const SRShaderCompileInfo& info, SRShader& shader) {
	const std::string pathStr = path;
	const std::string fileName = pathStr.substr(pathStr.find_last_of("/\\") + 1);

	std::ifstream file(path, std::ios::ate | std::ios::binary);
	if (!file.is_open()) {
		SRLOG_ERROR("Failed to open shader file %s", path);
		throw std::runtime_error("Failed to open file");
	}

	const size_t fileSize = static_cast<size_t>(file.tellg());
	std::vector<char> shaderCode(fileSize);

	file.seekg(0);
	file.read(shaderCode.data(), fileSize);
	file.close();
	shaderCode.push_back('\0');

	const slang::TargetDesc targetDesc = {
		.format = to_slang_target(m_ShaderPlatformInfo.target),
		.profile = m_GlobalSession->findProfile(m_ShaderPlatformInfo.profileName)
	};
	// TODO: Perhaps NOT hardcode paths for include?
	const char* includeDir = RES_DIR "Shaders/";
	const slang::SessionDesc sessionDesc = {
		.targets = &targetDesc,
		.targetCount = 1,
		.defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR,
		.searchPaths = &includeDir,
		.searchPathCount = 1
	};

	slang::ISession* session = nullptr;
	SR_SLANG_CHECK(m_GlobalSession->createSession(sessionDesc, &session), "Session creation");

	Slang::ComPtr<SlangCompileRequest> compileRequest = nullptr;
	SR_SLANG_CHECK(session->createCompileRequest(compileRequest.writeRef()), "Compile request creation");

	if (compileRequest == nullptr) {
		SRLOG_ERROR_CAT("SLANG", "Failed to create a compiler request");
		throw std::runtime_error("Slang error: Failed to create compiler request");
	}

	const char* args[] = {
		"-warnings-disable", "39001", // Disable warning for overlapping bindings
		#if defined(_DEBUG)
			"-O0",
			"-g2",
		#else
			"-O3",
		#endif
	};

	SR_SLANG_CHECK(compileRequest->processCommandLineArguments(args, std::size(args)), "Command line args processing");

	// TODO: Look into preprocessor definitions
	int tuIdx = compileRequest->addTranslationUnit(SLANG_SOURCE_LANGUAGE_SLANG, fileName.c_str());
	compileRequest->addTranslationUnitSourceString(tuIdx, fileName.c_str(), shaderCode.data()); // TODO: Fix. Includes do not work right now

	compileRequest->addEntryPoint(tuIdx, info.entryPoint, to_slang_stage(info.stage));
	SlangResult compileRes = compileRequest->compile();

	const char* diagnostics = compileRequest->getDiagnosticOutput();
	if (diagnostics && strlen(diagnostics) > 0) {
		if (SLANG_FAILED(compileRes)) {
			SRLOG_ERROR_CAT("SLANG", "Failed to compile %s. Diagnostics: %s", fileName.c_str(), diagnostics);
		}
		else {
			SRLOG_WARN_CAT("SLANG", "Successfully compiled %s, but generated diagnostics: %s", fileName.c_str(), diagnostics);
		}
	}
	SR_SLANG_CHECK(compileRes, "Shader compilation");

	Slang::ComPtr<slang::IModule> shaderModule;
	compileRequest->getModule(tuIdx, shaderModule.writeRef());

	Slang::ComPtr<slang::IEntryPoint> entryPoint;
	SR_SLANG_CHECK(shaderModule->getDefinedEntryPoint(0, entryPoint.writeRef()), "Get defined entrypoint");
	Slang::ComPtr<slang::IBlob> byteCode;
	SR_SLANG_CHECK(compileRequest->getEntryPointCodeBlob(0, 0, byteCode.writeRef()), "Get shader byte code");

	shader.byteCode.resize(byteCode->getBufferSize());
	std::memcpy(shader.byteCode.data(), byteCode->getBufferPointer(), byteCode->getBufferSize());
	session->release();
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
