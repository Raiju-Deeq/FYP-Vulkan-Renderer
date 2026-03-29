/**
 * @file main.cpp
 * @brief Application entry point — window creation and main loop.
 *
 * Initialises GLFW, creates the Vulkan context, swapchain, pipeline and
 * renderer in the correct order, then drives the per-frame loop until
 * the window is closed.  Tears everything down in reverse order on exit.
 *
 * @author Mohamed Deeq Mohamed
 * @date   2026-03-27
 *
 * @todo  Week 2 — replace the build-OK stub with the real init / loop /
 *                 destroy sequence once VulkanContext::init() is implemented.
 */

#include <iostream>

/**
 * @brief Application entry point.
 *
 * @return 0  on clean exit.
 * @return 1  if any initialisation step fails.
 */
int main()
{
    // TODO: Week 2 — glfwInit, glfwCreateWindow
    // TODO: Week 2 — VulkanContext::init(window)
    // TODO: Week 2 — SwapChain::init(ctx, w, h)
    // TODO: Week 2 — Pipeline::init(ctx, "vert.spv", "frag.spv", format)
    // TODO: Week 2 — Renderer::init(ctx, swap)
    // TODO: Week 2 — while (!glfwWindowShouldClose) { renderer.drawFrame(...) }
    // TODO: Week 2 — renderer.waitIdle(ctx) then destroy all in reverse

    std::cout << "FYP Vulkan Renderer — scaffold build OK (Week 1)\n";
    return 0;
}
