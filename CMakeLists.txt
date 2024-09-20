cmake_minimum_required(VERSION 3.6.0)

project(gl-testbench)

# Options
option(BUILD_OPENGL "Build OpenGL" ON)
option(BUILD_VULKAN "Build Vulkan" ON)

# Dependencies
set(DEPENDENCY_DIR ${CMAKE_HOME_DIRECTORY}/dependencies)
set(GLFW_INCLUDE_DIR ${DEPENDENCY_DIR}/glfw/include)
set(GLM_INCLUDE_DIR ${DEPENDENCY_DIR}/glm/)
set(TINYGLTF_INCLUDE_DIR ${DEPENDENCY_DIR}/tinygltf/)

add_subdirectory(dependencies/glfw)
add_subdirectory(dependencies/tinygltf)

# Settings
set(DCMAKE_GENERATOR_PLATFORM "x64")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Source files
set(SOURCE_DIR ${CMAKE_HOME_DIRECTORY}/src)
set(SOURCE_FILES
	${SOURCE_DIR}/enum_flags.h
    ${SOURCE_DIR}/main.cpp
    ${SOURCE_DIR}/platform.h
	${SOURCE_DIR}/frame_info.h

	# Data
	${SOURCE_DIR}/data/camera.cpp
	${SOURCE_DIR}/data/camera.h
	${SOURCE_DIR}/data/model.h

	# Entity Component System (ECS)
	${SOURCE_DIR}/ecs/components.h
	${SOURCE_DIR}/ecs/ecs.cpp
	${SOURCE_DIR}/ecs/ecs.h
	# Input

	${SOURCE_DIR}/input/input.cpp
	${SOURCE_DIR}/input/input.h

    # Graphics
    ${SOURCE_DIR}/graphics/gfx_device.h
    ${SOURCE_DIR}/graphics/gfx_types.h
	${SOURCE_DIR}/graphics/render_graph.cpp
	${SOURCE_DIR}/graphics/render_graph.h

	# Renderpasses
	${SOURCE_DIR}/graphics/renderpasses/gbuffer_pass.cpp
	${SOURCE_DIR}/graphics/renderpasses/gbuffer_pass.h
	${SOURCE_DIR}/graphics/renderpasses/lighting_pass.cpp
	${SOURCE_DIR}/graphics/renderpasses/lighting_pass.h

	# Vulkan
	${SOURCE_DIR}/graphics/vulkan/gfx_device_vulkan.cpp
	${SOURCE_DIR}/graphics/vulkan/gfx_device_vulkan.h
	${SOURCE_DIR}/graphics/vulkan/gfx_types_vulkan.h

	# Managers
	${SOURCE_DIR}/managers/asset_manager.cpp
	${SOURCE_DIR}/managers/asset_manager.h
)

# 1. Find Vulkan SDK
if (DEFINED VULKAN_SDK_PATH)
	set(Vulkan_INCLUDE_DIRS "${VULKAN_SDK_PATH}/Include") # 1.1 Make sure this include path is correct
	set(Vulkan_LIBRARIES "${VULKAN_SDK_PATH}/Lib") # 1.2 Make sure lib path is correct
	set(Vulkan_FOUND "True")
else()
	find_package(Vulkan REQUIRED) # throws error if could not find Vulkan
	message(STATUS "Found Vulkan: $ENV{VULKAN_SDK}")
endif()
if (NOT Vulkan_FOUND)
	message(FATAL_ERROR "Could not find Vulkan library!")
else()
	message(STATUS "Using Vulkan library at: ${Vulkan_LIBRARIES}")
endif()

add_executable(${CMAKE_PROJECT_NAME} ${SOURCE_FILES})

if (MSVC)
	set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES
		VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/$(Configuration)/"
	)
endif()

# Preprocessor definitions
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
	GLM_FORCE_LEFT_HANDED
	GLM_FORCE_RADIANS
	GLM_FORCE_DEPTH_ZERO_TO_ONE
	ENGINE_BASE_DIR="${CMAKE_HOME_DIRECTORY}/"
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${GLFW_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
	${Vulkan_INCLUDE_DIRS}
)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
    glfw
	tinygltf
	${Vulkan_LIBRARIES}
)
