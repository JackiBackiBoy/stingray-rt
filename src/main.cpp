#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include "platform.h"
#include "data/camera.h"
#include "input/input.h"
#include "graphics/gl/gfx_device_gl.h"

#include <glm/glm.hpp>

#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>

struct FrameInfo {
	Camera* camera = nullptr;
	float dt;
};

struct PerFrameData {
	glm::mat4 projectionMatrix = { 1.0f };
	glm::mat4 viewMatrix = { 1.0f };
};

struct Vertex {
	glm::vec3 position = {};
	glm::vec3 color = {};
};

GLOBAL_VARIABLE int g_FrameWidth = 1024;
GLOBAL_VARIABLE int g_FrameHeight = 512;
GLOBAL_VARIABLE GLFWwindow* g_Window = nullptr;
GLOBAL_VARIABLE std::unique_ptr<GFXDevice> g_GfxDevice = {};
GLOBAL_VARIABLE Shader g_VertexShader = {};
GLOBAL_VARIABLE Shader g_PixelShader = {};
GLOBAL_VARIABLE Pipeline g_Pipeline = {};

// TODO: For Vulkan and DX12, make frame-in-flight amount of these resources
GLOBAL_VARIABLE Buffer g_PerFrameDataBuffer = {};
GLOBAL_VARIABLE PerFrameData g_PerFrameData = {};

GLOBAL_VARIABLE Buffer g_VertexBuffer = {};
GLOBAL_VARIABLE Buffer g_IndexBuffer = {};
GLOBAL_VARIABLE Vertex vertices[] = {
	Vertex { { -0.5f, -0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f } },
	Vertex { {  0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f, 0.0f } },
	Vertex { {  0.5f,  0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f } },
	Vertex { { -0.5f,  0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f } },
};
GLOBAL_VARIABLE uint32_t indices[] = { 0, 1, 2, 2, 3, 0 };

GLOBAL_VARIABLE std::unique_ptr<Camera> g_Camera = {};
GLOBAL_VARIABLE FrameInfo g_FrameInfo = {};

// Callbacks
INTERNAL void resize_callback(GLFWwindow* window, int width, int height) {
	g_FrameWidth = width;
	g_FrameHeight = height;
	glViewport(0, 0, width, height);
}

INTERNAL void mouse_position_callback(GLFWwindow* window, double xpos, double ypos) {
	input::process_mouse_position(xpos, ypos);
}

INTERNAL void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
	input::process_mouse_event(button, action, mods);
}

INTERNAL void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
		glfwSetWindowShouldClose(g_Window, true);
	}

	input::process_key_event(key, scancode, action, mods);
}

INTERNAL int init_glfw(GLFWwindow** window) {
	// Initialize GLFW
	if (!glfwInit()) {
		std::cout << "Failed to initialize GLFW.\n";
		return 0;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	#ifdef __APPLE__
		glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	#endif

	*window = glfwCreateWindow(
		g_FrameWidth,
		g_FrameHeight,
		"gl-testbench (OpenGL)",
		nullptr,
		nullptr
	);

	if (*window == nullptr) {
		std::cout << "Failed to create GLFW window\n";
		glfwTerminate();
		return 0;
	}

	// Center window
	GLFWmonitor* monitor = glfwGetPrimaryMonitor();
	const GLFWvidmode* mode = glfwGetVideoMode(monitor);

	int screenSpaceWidth;
	int screenSpaceHeight;
	glfwGetWindowSize(*window, &screenSpaceWidth, &screenSpaceHeight);
	glfwSetWindowPos(
		*window,
		mode->width / 2 - screenSpaceWidth / 2,
		mode->height / 2 - screenSpaceHeight / 2
	);

	glfwMakeContextCurrent(*window);

	return 1;
}

INTERNAL int init_glad() {
	return gladLoadGL((GLADloadfunc)glfwGetProcAddress);
}

INTERNAL void init_gfx() {
	g_GfxDevice = std::make_unique<GFXDevice_GL>();
	
	g_GfxDevice->create_shader(ShaderStage::VERTEX, "shaders/gl/hello.vert.glsl", g_VertexShader);
	g_GfxDevice->create_shader(ShaderStage::PIXEL, "shaders/gl/hello.frag.glsl", g_PixelShader);

	const PipelineInfo pipelineInfo = {
		.vertexShader = &g_VertexShader,
		.pixelShader = &g_PixelShader,
		.inputLayout = {
			.elements = {
				{ "POSITION", Format::R32G32B32_FLOAT },
				{ "COLOR", Format::R32G32B32_FLOAT },
			}
		},
	};
	g_GfxDevice->create_pipeline(pipelineInfo, g_Pipeline);

	const BufferInfo perFrameDataBufferInfo = {
		.size = sizeof(PerFrameData),
		.stride = sizeof(PerFrameData),
		.usage = Usage::UPLOAD,
		.bindFlags = BindFlag::UNIFORM_BUFFER,
	};
	g_GfxDevice->create_buffer(perFrameDataBufferInfo, g_PerFrameDataBuffer, &g_PerFrameData);

	const BufferInfo vertexBufferInfo = {
		.size = sizeof(vertices),
		.stride = sizeof(Vertex),
		.bindFlags = BindFlag::VERTEX_BUFFER,
	};
	g_GfxDevice->create_buffer(vertexBufferInfo, g_VertexBuffer, vertices);

	const BufferInfo indexBufferInfo = {
		.size = sizeof(indices),
		.stride = sizeof(uint32_t),
		.bindFlags = BindFlag::INDEX_BUFFER,
	};
	g_GfxDevice->create_buffer(indexBufferInfo, g_IndexBuffer, indices);
}

INTERNAL void init_objects() {
	g_Camera = std::make_unique<Camera>(
		glm::vec3(0.0f, 0.0f, -1.0f),
		glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
		glm::radians(60.0f),
		(float)g_FrameWidth / g_FrameHeight,
		0.1f,
		100.0f
	);
}

INTERNAL void on_update(FrameInfo& frameInfo) {
	input::update();
	input::MouseState mouse = {};

	input::get_mouse_state(mouse);

	Camera& camera = *frameInfo.camera;
	const float cameraMoveSpeed = 5.0f;
	const float mouseSensitivity = 0.001f;

	if (mouse.buttons[2]) {
		glm::quat newOrientation = camera.getOrientation();

		if (mouse.deltaY != 0.0f) {
			newOrientation = newOrientation * glm::angleAxis(mouse.deltaY * mouseSensitivity, glm::vec3(1.0f, 0.0f, 0.0f)); // pitch
		}

		if (mouse.deltaX != 0.0f) {
			newOrientation = glm::angleAxis(mouse.deltaX * mouseSensitivity, glm::vec3(0.0f, 1.0f, 0.0f)) * newOrientation; // yaw
		}

		camera.setOrientation(newOrientation);
	}

	const glm::vec3 qRight = camera.getRight();
	const glm::vec3 qUp = camera.getUp();
	const glm::vec3 qForward = camera.getForward();

	glm::vec3 newPosition = camera.getPosition();

	if (input::is_key_down(GLFW_KEY_W)) {
		newPosition += qForward * cameraMoveSpeed * frameInfo.dt;
	}
	if (input::is_key_down(GLFW_KEY_A)) {
		newPosition -= qRight * cameraMoveSpeed * frameInfo.dt;
	}
	if (input::is_key_down(GLFW_KEY_S)) {
		newPosition -= qForward * cameraMoveSpeed * frameInfo.dt;
	}
	if (input::is_key_down(GLFW_KEY_D)) {
		newPosition += qRight * cameraMoveSpeed * frameInfo.dt;
	}

	if (input::is_key_down(GLFW_KEY_SPACE)) {
		newPosition.y += cameraMoveSpeed * frameInfo.dt;
	}
	if (input::is_key_down(GLFW_KEY_LEFT_CONTROL)) {
		newPosition.y -= cameraMoveSpeed * frameInfo.dt;
	}

	camera.setPosition(newPosition);

	const float aspectRatio = static_cast<float>(g_FrameWidth) / g_FrameHeight;
	camera.setAspectRatio(aspectRatio);

	camera.update();

	g_PerFrameData.projectionMatrix = camera.getProjMatrix();
	g_PerFrameData.viewMatrix = camera.getViewMatrix();
	g_GfxDevice->update_buffer(g_PerFrameDataBuffer, &g_PerFrameData);
}

int main() {
	if (!init_glfw(&g_Window)) { return -1; }
	if (!init_glad()) { return -1; }

	// Callbacks
	glfwSetFramebufferSizeCallback(g_Window, resize_callback);
	glfwSetCursorPosCallback(g_Window, mouse_position_callback);
	glfwSetMouseButtonCallback(g_Window, mouse_button_callback);
	glfwSetKeyCallback(g_Window, key_callback);

	init_gfx();
	init_objects();
	g_FrameInfo.camera = g_Camera.get();

	float deltaTime = 0.0f;
	float lastFrame = 0.0f;

	while (!glfwWindowShouldClose(g_Window)) {
		const float currentFrame = glfwGetTime();
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;
		
		// Update
		g_FrameInfo.dt = deltaTime;
		on_update(g_FrameInfo);

		// Rendering
		g_GfxDevice->begin_render_pass();
		{
			g_GfxDevice->bind_pipeline(g_Pipeline);
			g_GfxDevice->bind_uniform_buffer(g_PerFrameDataBuffer, 0);
			g_GfxDevice->bind_vertex_buffer(g_VertexBuffer);
			g_GfxDevice->bind_index_buffer(g_IndexBuffer);
			g_GfxDevice->draw_indexed(6, 0, 0);
		}
		g_GfxDevice->end_render_pass();

		glfwSwapBuffers(g_Window);
		glfwPollEvents();
	}

	// Shutdown
	glfwTerminate();
	return 0;
}
