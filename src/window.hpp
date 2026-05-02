/**
 * @file window.hpp
 * @brief GLFW window lifecycle wrapper for the Vulkan application.
 */

#ifndef FYP_VULKAN_RENDERER_WINDOW_HPP
#define FYP_VULKAN_RENDERER_WINDOW_HPP

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

/**
 * @class AppWindow
 * @brief Owns GLFW initialisation, the OS window handle and shutdown.
 */
class AppWindow
{
public:
    /**
     * @brief Initialises GLFW and creates a Vulkan-compatible window.
     * @param width Initial window width in screen coordinates.
     * @param height Initial window height in screen coordinates.
     * @param title OS window title.
     * @return true on success, false if GLFW or window creation fails.
     */
    bool init(int width, int height, const char* title);

    /**
     * @brief Destroys the GLFW window and terminates GLFW if initialised.
     */
    void destroy();

    /**
     * @brief Returns the raw GLFW window handle.
     * @return Pointer owned by AppWindow, or nullptr before init().
     */
    GLFWwindow* handle() const { return m_window; }

    /**
     * @brief Checks whether the OS window has requested close.
     * @return true if there is no window or close was requested.
     */
    bool shouldClose() const;

    /**
     * @brief Reads the current framebuffer size in physical pixels.
     * @param width Receives framebuffer width.
     * @param height Receives framebuffer height.
     */
    void framebufferSize(int& width, int& height) const;

    /**
     * @brief Pumps pending GLFW events without blocking.
     */
    static void pollEvents();

    /**
     * @brief Blocks until GLFW receives a new event.
     */
    static void waitEvents();

private:
    GLFWwindow* m_window = nullptr;
    bool        m_glfwInitialised = false;
};

#endif // FYP_VULKAN_RENDERER_WINDOW_HPP
