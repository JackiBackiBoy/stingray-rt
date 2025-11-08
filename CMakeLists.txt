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

# 1. FreeType
add_subdirectory(Dependencies/FreeType)
set_property(TARGET freetype PROPERTY FOLDER "Dependencies")

# 2. Volk
set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)
add_subdirectory(Dependencies/Volk)
set_property(TARGET volk PROPERTY FOLDER "Dependencies")

# 3. FastGLTF
add_subdirectory(Dependencies/FastGLTF)
set_property(TARGET fastgltf PROPERTY FOLDER "Dependencies")

# 4. D3D12 Memory Allocator
set(D3D12MA_AGILITY_SDK_DIRECTORY ${DEPENDENCY_DIR}/DXAgilitySDK CACHE STRING "Agility SDK Path for D3D12MA" FORCE)
add_subdirectory(Dependencies/D3D12MA)
set_property(TARGET D3D12MemoryAllocator PROPERTY FOLDER "Dependencies")

add_subdirectory(StingrayGraphics)
