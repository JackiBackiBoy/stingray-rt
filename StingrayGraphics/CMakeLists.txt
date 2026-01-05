cmake_minimum_required(VERSION 3.10)
project(StingrayGraphics LANGUAGES CXX)

# --- Build Options -------------------------------------------------------------------------------
option(BUILD_VULKAN "Build Vulkan" ON)
option(BUILD_DX12 "Build D3D12" ON)

# --- Settings ------------------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/Lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/Lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/Bin)

if(MSVC)
	set(CMAKE_CXX_FLAGS_RELEASE "/O2 /fp:fast")
else()
	set(CMAKE_CXX_FLAGS_RELEASE "-O3 -ffast-math")
endif()

# --- Dependencies --------------------------------------------------------------------------------
set(DEPENDENCY_DIR                 ${CMAKE_SOURCE_DIR}/Dependencies)
set(D3D12MA_INCLUDE_DIR            ${DEPENDENCY_DIR}/D3D12MA/include/)
set(DXSDK_BIN_DIR                  ${DEPENDENCY_DIR}/DXAgilitySDK/build/native/bin/x64/)
set(DXSDK_INCLUDE_DIR              ${DEPENDENCY_DIR}/DXAgilitySDK/build/native/include/)
set(DX_SHADER_COMPILER_BIN_DIR     ${DEPENDENCY_DIR}/DXShaderCompiler/bin/x64/)
set(DX_SHADER_COMPILER_INCLUDE_DIR ${DEPENDENCY_DIR}/DXShaderCompiler/inc/)
set(FASTGLTF_INCLUDE_DIR           ${DEPENDENCY_DIR}/FastGLTF/include/)
set(GLM_INCLUDE_DIR                ${DEPENDENCY_DIR}/GLM/)
set(IMGUI_INCLUDE_DIR              ${DEPENDENCY_DIR}/ImGui/)
set(MESH_OPTIMIZER_INCLUDE_DIR     ${DEPENDENCY_DIR}/MeshOptimizer/src/)
set(VMA_INCLUDE_DIR                ${DEPENDENCY_DIR}/VMA/include/)

# --- Source Files --------------------------------------------------------------------------------
set(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Source)
set(SOURCE_FILES
	${SOURCE_DIR}/Main.cpp

	# Core
	${SOURCE_DIR}/Core/EnumBitmaskOperators.h
	${SOURCE_DIR}/Core/Logger.cpp
	${SOURCE_DIR}/Core/Logger.h
	${SOURCE_DIR}/Core/StringTypes.cpp
	${SOURCE_DIR}/Core/StringTypes.h
	${SOURCE_DIR}/Core/Types.h
	${SOURCE_DIR}/Core/Window.cpp
	${SOURCE_DIR}/Core/Window.h

	# Core/System
	${SOURCE_DIR}/Core/System/MonitorEnumerator.cpp
	${SOURCE_DIR}/Core/System/MonitorEnumerator.h
	${SOURCE_DIR}/Core/System/Time.cpp
	${SOURCE_DIR}/Core/System/Time.h

	# Data
	${SOURCE_DIR}/Data/ArenaAllocator.cpp
	${SOURCE_DIR}/Data/ArenaAllocator.h
	${SOURCE_DIR}/Data/Camera.cpp
	${SOURCE_DIR}/Data/Camera.h
	${SOURCE_DIR}/Data/ComponentTypes.cpp
	${SOURCE_DIR}/Data/ComponentTypes.h
	${SOURCE_DIR}/Data/Model.cpp
	${SOURCE_DIR}/Data/Model.h
	${SOURCE_DIR}/Data/Scene.cpp
	${SOURCE_DIR}/Data/Scene.h
	${SOURCE_DIR}/Data/SparseSet.cpp
	${SOURCE_DIR}/Data/SparseSet.h

	# Graphics
	${SOURCE_DIR}/Graphics/FrameInfo.h
	${SOURCE_DIR}/Graphics/GraphicsDevice.cpp
	${SOURCE_DIR}/Graphics/GraphicsDevice.h
	${SOURCE_DIR}/Graphics/GraphicsTypes.h
	${SOURCE_DIR}/Graphics/RenderGraph.cpp
	${SOURCE_DIR}/Graphics/RenderGraph.h
	${SOURCE_DIR}/Graphics/ShaderCompiler.cpp
	${SOURCE_DIR}/Graphics/ShaderCompiler.h

	# Graphics/DX12
	${SOURCE_DIR}/Graphics/DX12/GraphicsDevice_DX12.cpp
	${SOURCE_DIR}/Graphics/DX12/GraphicsDevice_DX12.h
	${SOURCE_DIR}/Graphics/DX12/GraphicsHelpers_DX12.h
	${SOURCE_DIR}/Graphics/DX12/GraphicsTypes_DX12.cpp
	${SOURCE_DIR}/Graphics/DX12/GraphicsTypes_DX12.h

	# Graphics/Vulkan
	${SOURCE_DIR}/Graphics/Vulkan/GraphicsDevice_Vulkan.cpp
	${SOURCE_DIR}/Graphics/Vulkan/GraphicsDevice_Vulkan.h
	${SOURCE_DIR}/Graphics/Vulkan/GraphicsHelpers_Vulkan.h
	${SOURCE_DIR}/Graphics/Vulkan/GraphicsTypes_Vulkan.cpp
	${SOURCE_DIR}/Graphics/Vulkan/GraphicsTypes_Vulkan.h

	# Graphics/Renderpasses
	${SOURCE_DIR}/Graphics/Renderpasses/DepthPrepass.cpp
	${SOURCE_DIR}/Graphics/Renderpasses/DepthPrepass.h
	${SOURCE_DIR}/Graphics/Renderpasses/GBufferPass.cpp
	${SOURCE_DIR}/Graphics/Renderpasses/GBufferPass.h
	${SOURCE_DIR}/Graphics/Renderpasses/CompositionPass.cpp
	${SOURCE_DIR}/Graphics/Renderpasses/CompositionPass.h
	${SOURCE_DIR}/Graphics/Renderpasses/MeshletGenerationPass.cpp
	${SOURCE_DIR}/Graphics/Renderpasses/MeshletGenerationPass.h

	# Input
	${SOURCE_DIR}/Input/Input.cpp
	${SOURCE_DIR}/Input/Input.h

	# UI
	${SOURCE_DIR}/UI/Editor.cpp
	${SOURCE_DIR}/UI/Editor.h

	# Utilities
	${SOURCE_DIR}/Utilities/TextUtilities.h
)

# ------------------------------ Source Groupings ------------------------------
source_group("" FILES
	${SOURCE_DIR}/Main.cpp
)

source_group("Core" FILES
	${SOURCE_DIR}/Core/EnumBitmaskOperators.h
	${SOURCE_DIR}/Core/Logger.cpp
	${SOURCE_DIR}/Core/Logger.h
	${SOURCE_DIR}/Core/StringTypes.cpp
	${SOURCE_DIR}/Core/StringTypes.h
	${SOURCE_DIR}/Core/Types.h
	${SOURCE_DIR}/Core/Window.cpp
	${SOURCE_DIR}/Core/Window.h
)

source_group("Core/System" FILES
	${SOURCE_DIR}/Core/System/MonitorEnumerator.cpp
	${SOURCE_DIR}/Core/System/MonitorEnumerator.h
	${SOURCE_DIR}/Core/System/Time.cpp
	${SOURCE_DIR}/Core/System/Time.h
)

source_group("Data" FILES
	${SOURCE_DIR}/Data/ArenaAllocator.cpp
	${SOURCE_DIR}/Data/ArenaAllocator.h
	${SOURCE_DIR}/Data/Camera.cpp
	${SOURCE_DIR}/Data/Camera.h
	${SOURCE_DIR}/Data/ComponentTypes.cpp
	${SOURCE_DIR}/Data/ComponentTypes.h
	${SOURCE_DIR}/Data/Model.cpp
	${SOURCE_DIR}/Data/Model.h
	${SOURCE_DIR}/Data/Scene.cpp
	${SOURCE_DIR}/Data/Scene.h
	${SOURCE_DIR}/Data/SparseSet.cpp
	${SOURCE_DIR}/Data/SparseSet.h
)

source_group("Graphics" FILES
	${SOURCE_DIR}/Graphics/FrameInfo.h
	${SOURCE_DIR}/Graphics/GraphicsDevice.cpp
	${SOURCE_DIR}/Graphics/GraphicsDevice.h
	${SOURCE_DIR}/Graphics/GraphicsTypes.h
	${SOURCE_DIR}/Graphics/RenderGraph.cpp
	${SOURCE_DIR}/Graphics/RenderGraph.h
	${SOURCE_DIR}/Graphics/ShaderCompiler.cpp
	${SOURCE_DIR}/Graphics/ShaderCompiler.h
)

source_group("Graphics/DX12" FILES
	${SOURCE_DIR}/Graphics/DX12/GraphicsDevice_DX12.cpp
	${SOURCE_DIR}/Graphics/DX12/GraphicsDevice_DX12.h
	${SOURCE_DIR}/Graphics/DX12/GraphicsHelpers_DX12.h
	${SOURCE_DIR}/Graphics/DX12/GraphicsTypes_DX12.cpp
	${SOURCE_DIR}/Graphics/DX12/GraphicsTypes_DX12.h
)

source_group("Graphics/Vulkan" FILES
	${SOURCE_DIR}/Graphics/Vulkan/GraphicsDevice_Vulkan.cpp
	${SOURCE_DIR}/Graphics/Vulkan/GraphicsDevice_Vulkan.h
	${SOURCE_DIR}/Graphics/Vulkan/GraphicsHelpers_Vulkan.h
	${SOURCE_DIR}/Graphics/Vulkan/GraphicsTypes_Vulkan.cpp
	${SOURCE_DIR}/Graphics/Vulkan/GraphicsTypes_Vulkan.h
)

source_group("Graphics/Renderpasses" FILES
	${SOURCE_DIR}/Graphics/Renderpasses/DepthPrepass.cpp
	${SOURCE_DIR}/Graphics/Renderpasses/DepthPrepass.h
	${SOURCE_DIR}/Graphics/Renderpasses/GBufferPass.cpp
	${SOURCE_DIR}/Graphics/Renderpasses/GBufferPass.h
	${SOURCE_DIR}/Graphics/Renderpasses/CompositionPass.cpp
	${SOURCE_DIR}/Graphics/Renderpasses/CompositionPass.h
	${SOURCE_DIR}/Graphics/Renderpasses/MeshletGenerationPass.cpp
	${SOURCE_DIR}/Graphics/Renderpasses/MeshletGenerationPass.h
)

source_group("Input" FILES
	${SOURCE_DIR}/Input/Input.cpp
	${SOURCE_DIR}/Input/Input.h
)

source_group("UI" FILES
	${SOURCE_DIR}/UI/Editor.cpp
	${SOURCE_DIR}/UI/Editor.h
)

source_group("Utilities" FILES
	${SOURCE_DIR}/Utilities/TextUtilities.h
)

# ----------------------------- Compile Executable -----------------------------
add_executable(
	StingrayGraphics
	WIN32
	${SOURCE_FILES}
)

if (MSVC)
	target_compile_options(StingrayGraphics PRIVATE
		/W3
		/WX
	)
else()
	target_compile_options(StingrayGraphics PRIVATE
		-Wall
		-Wextra
		-Werror
	)
endif()

# ------------------------------- Target Sources -------------------------------
# ImGui
set(IMGUI_SOURCE_FILES
	${IMGUI_INCLUDE_DIR}imgui.cpp
	${IMGUI_INCLUDE_DIR}imgui_demo.cpp
	${IMGUI_INCLUDE_DIR}imgui_draw.cpp
	${IMGUI_INCLUDE_DIR}imgui_widgets.cpp
	${IMGUI_INCLUDE_DIR}imgui_tables.cpp
	${IMGUI_INCLUDE_DIR}backends/imgui_impl_dx12.cpp
	${IMGUI_INCLUDE_DIR}backends/imgui_impl_vulkan.cpp
	${IMGUI_INCLUDE_DIR}backends/imgui_impl_win32.cpp
)
source_group("ImGui" FILES ${IMGUI_SOURCE_FILES})

# Win32 resource file and application manifest
set(WIN32_RES_DIR ${CMAKE_SOURCE_DIR}/Resources/Win32/)
set_source_files_properties(${WIN32_RES_DIR}Resources.rc PROPERTIES LANGUAGE RC)
target_sources(StingrayGraphics PRIVATE
	${WIN32_RES_DIR}Resources.rc
	${IMGUI_SOURCE_FILES}
)

set_target_properties(StingrayGraphics PROPERTIES
	VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/Bin/$(Configuration)/"
)

# Preprocessor definitions
target_compile_definitions(StingrayGraphics PRIVATE
	GLM_FORCE_LEFT_HANDED
	GLM_FORCE_RADIANS
	GLM_FORCE_DEPTH_ZERO_TO_ONE
	VK_NO_PROTOTYPES
	IMGUI_IMPL_VULKAN_NO_PROTOTYPES
	IMGUI_IMPL_VULKAN_USE_VOLK
	ENGINE_BASE_DIR="${CMAKE_SOURCE_DIR}/"
	RES_DIR="${CMAKE_SOURCE_DIR}/Resources/"

	NOMINMAX
	UNICODE
	_UNICODE
	WIN32_LEAN_AND_MEAN
)

target_include_directories(StingrayGraphics PRIVATE
	${SOURCE_DIR}
	${D3D12MA_INCLUDE_DIR}
	${DXSDK_INCLUDE_DIR}
	${DX_SHADER_COMPILER_INCLUDE_DIR}
	${FASTGLTF_INCLUDE_DIR}
	${GLM_INCLUDE_DIR}
	${IMGUI_INCLUDE_DIR}
	${IMGUI_INCLUDE_DIR}backends/
	${MESH_OPTIMIZER_INCLUDE_DIR}
	${VMA_INCLUDE_DIR}
	${WIN32_RES_DIR}
)

target_link_libraries(StingrayGraphics
	PUBLIC
	D3D12
	D3D12MemoryAllocator
	DXGI
	DXGUID
	Dwmapi
	fastgltf
	meshoptimizer
	volk::volk
)

target_link_options(StingrayGraphics PRIVATE "/MANIFEST:NO")

# Copy DX12 Dependencies
add_custom_command(
	TARGET StingrayGraphics POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory
		"${DXSDK_BIN_DIR}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/D3D12"
	COMMAND ${CMAKE_COMMAND} -E copy
		"${DX_SHADER_COMPILER_BIN_DIR}/dxcompiler.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>"
	COMMAND ${CMAKE_COMMAND} -E copy
		"${DX_SHADER_COMPILER_BIN_DIR}/dxil.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>"

	COMMENT "Copying DirectX 12 Agility SDK and DirectX Shader Compiler"
)

add_custom_target(CopyDependenciesDX12 ALL DEPENDS StingrayGraphics
	COMMENT "Copying DirectX 12 Agility and DirectX Shader Compiler to runtime output directory"
)
