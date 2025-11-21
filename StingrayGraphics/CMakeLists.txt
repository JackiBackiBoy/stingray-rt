cmake_minimum_required(VERSION 3.10)
project(StingrayGraphics LANGUAGES CXX)

# -------------------------------- Build Options -------------------------------
option(BUILD_VULKAN "Build Vulkan" ON)
option(BUILD_DX12 "Build D3D12" ON)

# ---------------------------------- Settings ----------------------------------
set(CMAKE_GENERATOR_PLATFORM "x64")
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

# -------------------------------- Dependencies --------------------------------
set(DEPENDENCY_DIR ${CMAKE_SOURCE_DIR}/Dependencies)
set(D3D12MA_INCLUDE_DIR ${DEPENDENCY_DIR}/D3D12MA/include/)
set(DXSDK_BIN_DIR ${DEPENDENCY_DIR}/DXAgilitySDK/build/native/bin/x64/)
set(DXSDK_INCLUDE_DIR ${DEPENDENCY_DIR}/DXAgilitySDK/build/native/include/)
set(FASTGLTF_INCLUDE_DIR ${DEPENDENCY_DIR}/FastGLTF/include/)
set(GLM_INCLUDE_DIR ${DEPENDENCY_DIR}/GLM/)
set(IMGUI_INCLUDE_DIR ${DEPENDENCY_DIR}/ImGui/)
set(SLANG_INCLUDE_DIR ${DEPENDENCY_DIR}/Slang/include/)
set(SLANG_LIBRARY_DIR ${DEPENDENCY_DIR}/Slang/lib/)
set(VMA_INCLUDE_DIR ${DEPENDENCY_DIR}/VMA/include/)

# -------------------------------- Source Files --------------------------------
set(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Source)
set(SOURCE_FILES
	${SOURCE_DIR}/Main.cpp

	# Core
	${SOURCE_DIR}/Core/Logger.cpp
	${SOURCE_DIR}/Core/Logger.hpp
	${SOURCE_DIR}/Core/Window.cpp
	${SOURCE_DIR}/Core/Window.hpp

	# Core/System
	${SOURCE_DIR}/Core/System/MonitorEnumerator.cpp
	${SOURCE_DIR}/Core/System/MonitorEnumerator.hpp
	${SOURCE_DIR}/Core/System/Time.cpp
	${SOURCE_DIR}/Core/System/Time.hpp

	# Data
	${SOURCE_DIR}/Data/Camera.cpp
	${SOURCE_DIR}/Data/Camera.hpp
	${SOURCE_DIR}/Data/ComponentTypes.cpp
	${SOURCE_DIR}/Data/ComponentTypes.hpp
	${SOURCE_DIR}/Data/Model.cpp
	${SOURCE_DIR}/Data/Model.hpp
	${SOURCE_DIR}/Data/Scene.cpp
	${SOURCE_DIR}/Data/Scene.hpp
	${SOURCE_DIR}/Data/SparseSet.cpp
	${SOURCE_DIR}/Data/SparseSet.hpp

	# Graphics
	${SOURCE_DIR}/Graphics/FrameInfo.hpp
	${SOURCE_DIR}/Graphics/GraphicsDevice.hpp
	${SOURCE_DIR}/Graphics/GraphicsTypes.hpp
	${SOURCE_DIR}/Graphics/RenderGraph.cpp
	${SOURCE_DIR}/Graphics/RenderGraph.hpp
	${SOURCE_DIR}/Graphics/ShaderCompiler.cpp
	${SOURCE_DIR}/Graphics/ShaderCompiler.hpp

	# Graphics/DX12
	${SOURCE_DIR}/Graphics/DX12/GraphicsDevice_DX12.cpp
	${SOURCE_DIR}/Graphics/DX12/GraphicsDevice_DX12.hpp
	${SOURCE_DIR}/Graphics/DX12/GraphicsHelpers_DX12.hpp
	${SOURCE_DIR}/Graphics/DX12/GraphicsTypes_DX12.cpp
	${SOURCE_DIR}/Graphics/DX12/GraphicsTypes_DX12.hpp

	# Graphics/Vulkan
	${SOURCE_DIR}/Graphics/Vulkan/GraphicsDevice_Vulkan.cpp
	${SOURCE_DIR}/Graphics/Vulkan/GraphicsDevice_Vulkan.hpp
	${SOURCE_DIR}/Graphics/Vulkan/GraphicsHelpers_Vulkan.hpp
	${SOURCE_DIR}/Graphics/Vulkan/GraphicsTypes_Vulkan.cpp
	${SOURCE_DIR}/Graphics/Vulkan/GraphicsTypes_Vulkan.hpp

	# Graphics/Renderpasses
	${SOURCE_DIR}/Graphics/Renderpasses/DepthPrepass.cpp
	${SOURCE_DIR}/Graphics/Renderpasses/DepthPrepass.hpp
	${SOURCE_DIR}/Graphics/Renderpasses/GBufferPass.cpp
	${SOURCE_DIR}/Graphics/Renderpasses/GBufferPass.hpp
	${SOURCE_DIR}/Graphics/Renderpasses/CompositionPass.cpp
	${SOURCE_DIR}/Graphics/Renderpasses/CompositionPass.hpp

	# Input
	${SOURCE_DIR}/Input/Input.cpp
	${SOURCE_DIR}/Input/Input.hpp

	# UI
	${SOURCE_DIR}/UI/Editor.cpp
	${SOURCE_DIR}/UI/Editor.hpp

	# Utilities
	${SOURCE_DIR}/Utilities/TextUtilities.hpp
)

# ------------------------------ Source Groupings ------------------------------
source_group("" FILES
	${SOURCE_DIR}/Main.cpp
)

source_group("Core" FILES
	${SOURCE_DIR}/Core/Logger.cpp
	${SOURCE_DIR}/Core/Logger.hpp
	${SOURCE_DIR}/Core/Window.cpp
	${SOURCE_DIR}/Core/Window.hpp
)

source_group("Core/System" FILES
	${SOURCE_DIR}/Core/System/MonitorEnumerator.cpp
	${SOURCE_DIR}/Core/System/MonitorEnumerator.hpp
	${SOURCE_DIR}/Core/System/Time.cpp
	${SOURCE_DIR}/Core/System/Time.hpp
)

source_group("Data" FILES
	${SOURCE_DIR}/Data/Camera.cpp
	${SOURCE_DIR}/Data/Camera.hpp
	${SOURCE_DIR}/Data/ComponentTypes.cpp
	${SOURCE_DIR}/Data/ComponentTypes.hpp
	${SOURCE_DIR}/Data/Model.cpp
	${SOURCE_DIR}/Data/Model.hpp
	${SOURCE_DIR}/Data/Scene.cpp
	${SOURCE_DIR}/Data/Scene.hpp
	${SOURCE_DIR}/Data/SparseSet.cpp
	${SOURCE_DIR}/Data/SparseSet.hpp
)

source_group("Graphics" FILES
	${SOURCE_DIR}/Graphics/FrameInfo.hpp
	${SOURCE_DIR}/Graphics/GraphicsDevice.hpp
	${SOURCE_DIR}/Graphics/GraphicsTypes.hpp
	${SOURCE_DIR}/Graphics/RenderGraph.cpp
	${SOURCE_DIR}/Graphics/RenderGraph.hpp
	${SOURCE_DIR}/Graphics/ShaderCompiler.cpp
	${SOURCE_DIR}/Graphics/ShaderCompiler.hpp
)

source_group("Graphics/DX12" FILES
	${SOURCE_DIR}/Graphics/DX12/GraphicsDevice_DX12.cpp
	${SOURCE_DIR}/Graphics/DX12/GraphicsDevice_DX12.hpp
	${SOURCE_DIR}/Graphics/DX12/GraphicsHelpers_DX12.hpp
	${SOURCE_DIR}/Graphics/DX12/GraphicsTypes_DX12.cpp
	${SOURCE_DIR}/Graphics/DX12/GraphicsTypes_DX12.hpp
)

source_group("Graphics/Vulkan" FILES
	${SOURCE_DIR}/Graphics/Vulkan/GraphicsDevice_Vulkan.cpp
	${SOURCE_DIR}/Graphics/Vulkan/GraphicsDevice_Vulkan.hpp
	${SOURCE_DIR}/Graphics/Vulkan/GraphicsHelpers_Vulkan.hpp
	${SOURCE_DIR}/Graphics/Vulkan/GraphicsTypes_Vulkan.cpp
	${SOURCE_DIR}/Graphics/Vulkan/GraphicsTypes_Vulkan.hpp
)

source_group("Graphics/Renderpasses" FILES
	${SOURCE_DIR}/Graphics/Renderpasses/DepthPrepass.cpp
	${SOURCE_DIR}/Graphics/Renderpasses/DepthPrepass.hpp
	${SOURCE_DIR}/Graphics/Renderpasses/GBufferPass.cpp
	${SOURCE_DIR}/Graphics/Renderpasses/GBufferPass.hpp
	${SOURCE_DIR}/Graphics/Renderpasses/CompositionPass.cpp
	${SOURCE_DIR}/Graphics/Renderpasses/CompositionPass.hpp
)

source_group("Input" FILES
	${SOURCE_DIR}/Input/Input.cpp
	${SOURCE_DIR}/Input/Input.hpp
)

source_group("UI" FILES
	${SOURCE_DIR}/UI/Editor.cpp
	${SOURCE_DIR}/UI/Editor.hpp
)

source_group("Utilities" FILES
	${SOURCE_DIR}/Utilities/TextUtilities.hpp
)

# ----------------------------- Compile Executable -----------------------------
add_executable(
	StingrayGraphics
	WIN32
	${SOURCE_FILES}
)

target_compile_options(StingrayGraphics PRIVATE
	/W4
)

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
	${FASTGLTF_INCLUDE_DIR}
	${GLM_INCLUDE_DIR}
	${IMGUI_INCLUDE_DIR}
	${IMGUI_INCLUDE_DIR}backends/
	${SLANG_INCLUDE_DIR}
	${VMA_INCLUDE_DIR}
	${WIN32_RES_DIR}
)

target_link_directories(StingrayGraphics PRIVATE
	${SLANG_LIBRARY_DIR}
)

target_link_libraries(StingrayGraphics
	PUBLIC
	D3D12
	D3D12MemoryAllocator
	DXGI
	DXGUID
	Dwmapi
	fastgltf
	volk::volk
	slang
)

target_link_options(StingrayGraphics PRIVATE "/MANIFEST:NO")

# Copy DX12 Dependencies
add_custom_command(
	TARGET StingrayGraphics POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory
		"${DXSDK_BIN_DIR}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/D3D12"

	COMMENT "Copying DirectX 12 Agility SDK to Bin/$<CONFIG>"
)

add_custom_target(CopyDependenciesDX12 ALL DEPENDS StingrayGraphics
	COMMENT "Copying DirectX 12 Agility to runtime output directory"
)
