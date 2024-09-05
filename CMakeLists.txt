cmake_minimum_required(VERSION 3.6.0)

project(gl-testbench)

# Dependencies
# Set options for glad
set(DEPENDENCY_DIR ${CMAKE_HOME_DIRECTORY}/dependencies)
set(GLFW_INCLUDE_DIR ${DEPENDENCY_DIR}/glfw/include)
set(GLM_INCLUDE_DIR ${DEPENDENCY_DIR}/glm/)

add_subdirectory(dependencies/glad/cmake)
glad_add_library(glad_gl_core_41 REPRODUCIBLE STATIC LOADER API gl:core=4.1)

add_subdirectory(dependencies/glfw)

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

	# Data
	${SOURCE_DIR}/data/camera.cpp
	${SOURCE_DIR}/data/camera.h

	# Input
	${SOURCE_DIR}/input/input.cpp
	${SOURCE_DIR}/input/input.h

    # Graphics
    ${SOURCE_DIR}/graphics/gfx_device.h
    ${SOURCE_DIR}/graphics/gfx_types.h
    ${SOURCE_DIR}/graphics/gl/gfx_device_gl.cpp
    ${SOURCE_DIR}/graphics/gl/gfx_device_gl.h
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
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${GLFW_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
    glad_gl_core_41
    glfw
)

# Post-build
add_custom_target(copy_shaders ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_HOME_DIRECTORY}/shaders
    $<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>/shaders
    COMMENT "Copying shaders directory..."
)

