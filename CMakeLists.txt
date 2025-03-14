cmake_minimum_required(VERSION 3.10)
project(stingray)

# -------------------------------- Build Options -------------------------------
option(BUILD_VULKAN "Build Vulkan" ON)
option(BUILD_DX12 "Build D3D12" OFF) # TODO: D3D12 backend not yet implemented

# ---------------------------------- Settings ----------------------------------
set(DCMAKE_GENERATOR_PLATFORM "x64")
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
set(FREETYPE_INCLUDE_DIR ${DEPENDENCY_DIR}/FreeType/include/)
set(GLM_INCLUDE_DIR ${DEPENDENCY_DIR}/GLM/)
set(TINYGLTF_INCLUDE_DIR ${DEPENDENCY_DIR}/TinyGLTF/)

# 1. FreeType
add_subdirectory(Dependencies/FreeType)
set_property(TARGET freetype PROPERTY FOLDER "Dependencies")

# 2. TinyGLTF
set(TINYGLTF_BUILD_LOADER_EXAMPLE OFF CACHE INTERNAL "Enable loader example")
add_subdirectory(Dependencies/TinyGLTF)
set_property(TARGET tinygltf PROPERTY FOLDER "Dependencies")

# 3. Volk
set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)
add_subdirectory(Dependencies/Volk)
set_property(TARGET volk PROPERTY FOLDER "Dependencies")

# -------------------------------- Source Files --------------------------------
set(SOURCE_DIR ${CMAKE_SOURCE_DIR}/Source)
set(SOURCE_FILES
	${SOURCE_DIR}/main.cpp

	# Core
	${SOURCE_DIR}/Core/EnumFlags.h
	${SOURCE_DIR}/Core/Platform.h
	${SOURCE_DIR}/Core/FrameInfo.h
	${SOURCE_DIR}/Core/Window.h
	${SOURCE_DIR}/Core/WindowWin32.cpp

	# Data
	${SOURCE_DIR}/Data/Camera.cpp
	${SOURCE_DIR}/Data/Camera.h
	${SOURCE_DIR}/Data/Font.cpp
	${SOURCE_DIR}/Data/Font.h
	${SOURCE_DIR}/Data/Model.h
	${SOURCE_DIR}/Data/Scene.cpp
	${SOURCE_DIR}/Data/Scene.h

	# Entity Component System (ECS)
	${SOURCE_DIR}/ECS/Components.h
	${SOURCE_DIR}/ECS/ECS.cpp
	${SOURCE_DIR}/ECS/ECS.h

	# Graphics
	${SOURCE_DIR}/Graphics/GraphicsDevice.h
	${SOURCE_DIR}/Graphics/GraphicsTypes.h
	${SOURCE_DIR}/Graphics/RenderGraph.cpp
	${SOURCE_DIR}/Graphics/RenderGraph.h
	
	# Graphics/Renderpasses
	${SOURCE_DIR}/Graphics/Renderpasses/FullscreenTriPass.cpp
	${SOURCE_DIR}/Graphics/Renderpasses/FullscreenTriPass.h
	${SOURCE_DIR}/Graphics/Renderpasses/RayTracingPass.cpp
	${SOURCE_DIR}/Graphics/Renderpasses/RayTracingPass.h
	${SOURCE_DIR}/Graphics/Renderpasses/UIPass.cpp
	${SOURCE_DIR}/Graphics/Renderpasses/UIPass.h

	# Graphics/Vulkan
	${SOURCE_DIR}/Graphics/Vulkan/GraphicsDeviceVulkan.cpp
	${SOURCE_DIR}/Graphics/Vulkan/GraphicsDeviceVulkan.h
	${SOURCE_DIR}/Graphics/Vulkan/GraphicsTypesVulkan.h

	# Input
	${SOURCE_DIR}/Input/Input.cpp
	${SOURCE_DIR}/Input/Input.h

	# Managers
	${SOURCE_DIR}/Managers/AssetManager.cpp
	${SOURCE_DIR}/Managers/AssetManager.h
	${SOURCE_DIR}/Managers/MaterialManager.cpp
	${SOURCE_DIR}/Managers/MaterialManager.h

	# Math
	${SOURCE_DIR}/Math/SRMath.h

	# UI
	${SOURCE_DIR}/UI/UIContext.cpp
	${SOURCE_DIR}/UI/UIContext.h
)

# ------------------------------ Source Groupings ------------------------------
source_group("" FILES
	${SOURCE_DIR}/main.cpp
)

source_group("Core" FILES
	${SOURCE_DIR}/Core/EnumFlags.h
	${SOURCE_DIR}/Core/Platform.h
	${SOURCE_DIR}/Core/FrameInfo.h
	${SOURCE_DIR}/Core/Window.h
	${SOURCE_DIR}/Core/WindowWin32.cpp
)

source_group("Data" FILES
	${SOURCE_DIR}/Data/Camera.cpp
	${SOURCE_DIR}/Data/Camera.h
	${SOURCE_DIR}/Data/Font.cpp
	${SOURCE_DIR}/Data/Font.h
	${SOURCE_DIR}/Data/Model.h
	${SOURCE_DIR}/Data/Scene.cpp
	${SOURCE_DIR}/Data/Scene.h
)

source_group("ECS" FILES
	${SOURCE_DIR}/ECS/Components.h
	${SOURCE_DIR}/ECS/ECS.cpp
	${SOURCE_DIR}/ECS/ECS.h
)

source_group("Graphics" FILES
	${SOURCE_DIR}/Graphics/GraphicsDevice.h
	${SOURCE_DIR}/Graphics/GraphicsTypes.h
	${SOURCE_DIR}/Graphics/RenderGraph.cpp
	${SOURCE_DIR}/Graphics/RenderGraph.h
)

source_group("Graphics/Renderpasses" FILES
	${SOURCE_DIR}/Graphics/Renderpasses/FullscreenTriPass.cpp
	${SOURCE_DIR}/Graphics/Renderpasses/FullscreenTriPass.h
	${SOURCE_DIR}/Graphics/Renderpasses/RayTracingPass.cpp
	${SOURCE_DIR}/Graphics/Renderpasses/RayTracingPass.h
	${SOURCE_DIR}/Graphics/Renderpasses/UIPass.cpp
	${SOURCE_DIR}/Graphics/Renderpasses/UIPass.h
)

source_group("Graphics/Vulkan" FILES
	${SOURCE_DIR}/Graphics/Vulkan/GraphicsDeviceVulkan.cpp
	${SOURCE_DIR}/Graphics/Vulkan/GraphicsDeviceVulkan.h
	${SOURCE_DIR}/Graphics/Vulkan/GraphicsTypesVulkan.h
)

source_group("Input" FILES
	${SOURCE_DIR}/Input/Input.cpp
	${SOURCE_DIR}/Input/Input.h
)

source_group("Managers" FILES
	${SOURCE_DIR}/Managers/AssetManager.cpp
	${SOURCE_DIR}/Managers/AssetManager.h
	${SOURCE_DIR}/Managers/MaterialManager.cpp
	${SOURCE_DIR}/Managers/MaterialManager.h
)

source_group("Math" FILES
	${SOURCE_DIR}/Math/SRMath.h
)

source_group("UI" FILES
	${SOURCE_DIR}/UI/UIContext.cpp
	${SOURCE_DIR}/UI/UIContext.h
)

# ----------------------------- Compile Executable -----------------------------
add_executable(
	${CMAKE_PROJECT_NAME}
	WIN32
	${SOURCE_FILES}
)

target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE
	/W4
)

# Win32 resource file and application manifest
set(WIN32_RES_DIR ${CMAKE_SOURCE_DIR}/Resources/Win32/)
set_source_files_properties(${WIN32_RES_DIR}Resources.rc PROPERTIES LANGUAGE RC)
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
	${WIN32_RES_DIR}app.manifest
	${WIN32_RES_DIR}Resources.rc
)

set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES
	VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/Bin/$(Configuration)/"
)

# Preprocessor definitions
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
	GLM_FORCE_LEFT_HANDED
	GLM_FORCE_RADIANS
	GLM_FORCE_DEPTH_ZERO_TO_ONE
	ENGINE_BASE_DIR="${CMAKE_SOURCE_DIR}/"
	ENGINE_RES_DIR="${CMAKE_SOURCE_DIR}/Resources/"

	NOMINMAX
	UNICODE
	_UNICODE
	WIN32_LEAN_AND_MEAN
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
	${SOURCE_DIR}
	${FREETYPE_INCLUDE_DIR}
	${GLM_INCLUDE_DIR}
	${WIN32_RES_DIR}
)

target_link_libraries(${CMAKE_PROJECT_NAME}
	PUBLIC
	volk::volk

	PRIVATE
	freetype
	tinygltf
)
