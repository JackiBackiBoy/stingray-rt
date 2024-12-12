cmake_minimum_required(VERSION 3.6.0)
project(stingray)

# -------------------------------- Build Options -------------------------------
option(BUILD_VULKAN "Build Vulkan" ON)

# ---------------------------------- Settings ----------------------------------
set(DCMAKE_GENERATOR_PLATFORM "x64")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /fp:fast")
else()
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -ffast-math")
endif()

# -------------------------------- Dependencies --------------------------------
set(DEPENDENCY_DIR ${CMAKE_HOME_DIRECTORY}/dependencies)
set(FREETYPE_INCLUDE_DIR ${DEPENDENCY_DIR}/freetype/include/)
set(GLFW_INCLUDE_DIR ${DEPENDENCY_DIR}/glfw/include)
set(GLM_INCLUDE_DIR ${DEPENDENCY_DIR}/glm/)
set(TINYGLTF_INCLUDE_DIR ${DEPENDENCY_DIR}/tinygltf/)

add_subdirectory(dependencies/freetype)
add_subdirectory(dependencies/tinygltf)
add_subdirectory(dependencies/volk)
add_subdirectory(dependencies/glfw)

# -------------------------------- Source Files --------------------------------
set(SOURCE_DIR ${CMAKE_HOME_DIRECTORY}/src)
set(SOURCE_FILES
	${SOURCE_DIR}/enum_flags.h
    ${SOURCE_DIR}/main.cpp
    ${SOURCE_DIR}/platform.h
	${SOURCE_DIR}/frame_info.h

	# Data
	${SOURCE_DIR}/data/camera.cpp
	${SOURCE_DIR}/data/camera.h
	${SOURCE_DIR}/data/font.cpp
	${SOURCE_DIR}/data/font.h
	${SOURCE_DIR}/data/model.h
	${SOURCE_DIR}/data/scene.cpp
	${SOURCE_DIR}/data/scene.h

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

	# Math
	${SOURCE_DIR}/math/sr_math.h

	# Renderpasses
	${SOURCE_DIR}/graphics/renderpasses/fullscreen_tri_pass.cpp
	${SOURCE_DIR}/graphics/renderpasses/fullscreen_tri_pass.h
	${SOURCE_DIR}/graphics/renderpasses/ray_tracing_pass.cpp
	${SOURCE_DIR}/graphics/renderpasses/ray_tracing_pass.h
	${SOURCE_DIR}/graphics/renderpasses/ui_pass.cpp
	${SOURCE_DIR}/graphics/renderpasses/ui_pass.h

	# Vulkan
	${SOURCE_DIR}/graphics/vulkan/gfx_device_vulkan.cpp
	${SOURCE_DIR}/graphics/vulkan/gfx_device_vulkan.h
	${SOURCE_DIR}/graphics/vulkan/gfx_types_vulkan.h

	# Managers
	${SOURCE_DIR}/managers/asset_manager.cpp
	${SOURCE_DIR}/managers/asset_manager.h
)

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
	ENGINE_RES_DIR="${CMAKE_HOME_DIRECTORY}/resources/"
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
	${FREETYPE_INCLUDE_DIR}
    ${GLFW_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
)

target_link_libraries(${CMAKE_PROJECT_NAME}
	PUBLIC
	volk::volk

	PRIVATE
	freetype
    glfw
	tinygltf
)
