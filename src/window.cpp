/**
 * @file window.cpp
 * @brief Implementation of the GLFW window wrapper.
 */

#include "window.hpp"

#include <spdlog/spdlog.h>

bool AppWindow::init(int width, int height, const char* title)
{
    if (!glfwInit()) {
        spdlog::error("AppWindow: glfwInit failed");
        return false;
    }
    m_glfwInitialised = true;

    // GLFW_NO_API: do not create an OpenGL context. Vulkan manages rendering.
    // GLFW_RESIZABLE: allow resize; the application rebuilds the swapchain.
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    m_window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!m_window) {
        spdlog::error("AppWindow: glfwCreateWindow failed");
        destroy();
        return false;
    }

    return true;
}

void AppWindow::destroy()
{
    // I keep GLFW teardown beside the window so Application does not need to
    // remember the exact shutdown order.
    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }

    if (m_glfwInitialised) {
        glfwTerminate();
        m_glfwInitialised = false;
    }
}

bool AppWindow::shouldClose() const
{
    return m_window == nullptr || glfwWindowShouldClose(m_window);
}

void AppWindow::framebufferSize(int& width, int& height) const
{
    glfwGetFramebufferSize(m_window, &width, &height);
}

void AppWindow::pollEvents()
{
    glfwPollEvents();
}

void AppWindow::waitEvents()
{
    glfwWaitEvents();
}
