#include "platform.h"
#include "graphics/gl/gfx_device_gl.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <iostream>
#include <memory>

struct Vertex {
    glm::vec3 position = {};
};

GLOBAL_VARIABLE constexpr int WIDTH = 1024;
GLOBAL_VARIABLE constexpr int HEIGHT = 512;
GLOBAL_VARIABLE GLFWwindow* g_Window = nullptr;
GLOBAL_VARIABLE std::unique_ptr<GFXDevice> g_GfxDevice = {};
GLOBAL_VARIABLE Shader g_VertexShader = {};
GLOBAL_VARIABLE Shader g_PixelShader = {};
GLOBAL_VARIABLE Pipeline g_Pipeline = {};

GLOBAL_VARIABLE Buffer g_VertexBuffer = {};
GLOBAL_VARIABLE Vertex vertices[] = {
    Vertex { { -0.5f, -0.5f, 0.0f } },
    Vertex { {  0.5f, -0.5f, 0.0f } },
    Vertex { {  0.0f,  0.5f, 0.0f } },
};

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
        WIDTH,
        HEIGHT,
        "gl-testbench (OpenGL)",
        nullptr,
        nullptr
    );

    if (*window == nullptr) {
        std::cout << "Failed to create GLFW window\n";
        glfwTerminate();
        return 0;
    }

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
                { "POSITION", Format::R32G32B32_FLOAT }
            }
        },
    };
    g_GfxDevice->create_pipeline(pipelineInfo, g_Pipeline);

    const BufferInfo vertexBufferInfo = {
        .size = sizeof(vertices),
        .stride = sizeof(Vertex),
        .bindFlags = BindFlag::VERTEX_BUFFER,
    };
    g_GfxDevice->create_buffer(vertexBufferInfo, g_VertexBuffer, vertices);
}

INTERNAL void resize_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

int main() {
    if (!init_glfw(&g_Window)) { return -1; }
    if (!init_glad()) { return -1; }

    // Callbacks
    glfwSetFramebufferSizeCallback(g_Window, resize_callback);

    init_gfx();

    while (!glfwWindowShouldClose(g_Window)) {
        if (glfwGetKey(g_Window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(g_Window, true);
        }

        // Rendering
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        g_GfxDevice->begin_render_pass();
        {
            g_GfxDevice->bind_pipeline(g_Pipeline);
            g_GfxDevice->bind_vertex_buffer(g_VertexBuffer);
            g_GfxDevice->draw(3, 0);
        }
        g_GfxDevice->end_render_pass();

        glfwSwapBuffers(g_Window);
        glfwPollEvents();
    }

    // Shutdown
    glfwTerminate();
    return 0;
}
