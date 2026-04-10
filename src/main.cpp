/**
 * @file main.cpp
 * @brief Application entry point - window creation, init sequence and main loop.
 *
 * Wires together the four core modules in the order their dependencies require:
 *   VulkanContext → SwapChain → Pipeline → Renderer
 *
 * The main loop runs until the GLFW window is closed.  On each iteration it
 * polls window events and calls Renderer::drawFrame().  If drawFrame() returns
 * false (swapchain out of date after a window resize), the swapchain and
 * pipeline are rebuilt immediately before the next frame.
 *
 * Teardown happens in strict reverse construction order so every child object
 * is destroyed before the parent it depends on.
 *
 * @note  Shader .spv paths are relative to the working directory.  Run the
 *        executable from the project root so "shaders/triangle.vert.spv" resolves
 *        correctly (CMake compiles shaders to ${sourceDir}/shaders/).
 *
 * @author Mohamed Deeq Mohamed (P2884884)
 * @date   2026-04-10
 */

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "VulkanContext.h"
#include "SwapChain.h"
#include "Pipeline.h"
#include "Renderer.h"

#include <spdlog/spdlog.h>
#include <cstdlib>

// ─── Constants ────────────────────────────────────────────────────────────────

static constexpr int  WINDOW_WIDTH  = 1280;
static constexpr int  WINDOW_HEIGHT = 720;
static constexpr char WINDOW_TITLE[] = "FYP Vulkan Renderer — M1 Triangle";

// Paths are relative to the working directory (project root).
// CMake compiles shaders/triangle.vert → shaders/triangle.vert.spv automatically.
static constexpr char VERT_SPV[] = "shaders/triangle.vert.spv";
static constexpr char FRAG_SPV[] = "shaders/triangle.frag.spv";

// ─── Entry point ──────────────────────────────────────────────────────────────

/**
 * @brief Application entry point.
 *
 * @return EXIT_SUCCESS (0) on clean shutdown.
 * @return EXIT_FAILURE (1) if any initialisation step fails.
 */
int main()
{
    spdlog::set_level(spdlog::level::info);
    spdlog::info("=== FYP Vulkan Renderer — Milestone 1 ===");

    // ── GLFW ──────────────────────────────────────────────────────────────
    if (!glfwInit()) {
        spdlog::error("main: glfwInit failed");
        return EXIT_FAILURE;
    }

    // GLFW_NO_API tells GLFW not to create an OpenGL context — we drive Vulkan
    // ourselves.  GLFW_RESIZABLE enables OS-level window resize; we handle the
    // resulting swapchain rebuild in the loop below.
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT,
                                          WINDOW_TITLE, nullptr, nullptr);
    if (!window) {
        spdlog::error("main: glfwCreateWindow failed");
        glfwTerminate();
        return EXIT_FAILURE;
    }

    // ── VulkanContext ─────────────────────────────────────────────────────
    // Creates the VkInstance (with validation layers), selects a GPU, creates
    // the VkDevice and retrieves the graphics queue.
    VulkanContext ctx;
    if (!ctx.init(window)) {
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    // ── SwapChain ─────────────────────────────────────────────────────────
    // Use the framebuffer size (physical pixels), not the window size (logical
    // pixels), to handle high-DPI displays correctly.
    int fbWidth = 0, fbHeight = 0;
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);

    SwapChain swap;
    if (!swap.init(ctx, static_cast<uint32_t>(fbWidth),
                        static_cast<uint32_t>(fbHeight))) {
        ctx.destroy();
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    // ── Pipeline ──────────────────────────────────────────────────────────
    // Loads SPIR-V shaders and builds the Dynamic Rendering graphics pipeline.
    // The colour format must match the swapchain so the pipeline targets the
    // correct attachment format.
    Pipeline pipeline;
    if (!pipeline.init(ctx, VERT_SPV, FRAG_SPV, swap.format())) {
        swap.destroy(ctx);
        ctx.destroy();
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    // ── Renderer ──────────────────────────────────────────────────────────
    // Creates the command pool, command buffers, semaphores and fences for the
    // double-buffered render loop.
    Renderer renderer;
    if (!renderer.init(ctx, swap)) {
        pipeline.destroy(ctx);
        swap.destroy(ctx);
        ctx.destroy();
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    spdlog::info("main: all modules initialised — entering render loop");

    // ── Render loop ───────────────────────────────────────────────────────
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Skip rendering while the window is minimised (framebuffer size = 0).
        // Attempting to acquire/present with a zero-extent swapchain is invalid.
        glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
        if (fbWidth == 0 || fbHeight == 0) {
            continue;
        }

        if (!renderer.drawFrame(ctx, swap, pipeline)) {
            // drawFrame returns false on VK_ERROR_OUT_OF_DATE_KHR (window resize)
            // or VK_SUBOPTIMAL_KHR.  Wait for all in-flight work to finish before
            // rebuilding so we don't destroy resources the GPU is still reading.
            renderer.waitIdle(ctx);

            glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
            while (fbWidth == 0 || fbHeight == 0) {
                // Window is minimised — wait for a resize event before rebuilding.
                glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
                glfwWaitEvents();
            }

            // Rebuild swapchain for the new window size.
            if (!swap.rebuild(ctx, static_cast<uint32_t>(fbWidth),
                                    static_cast<uint32_t>(fbHeight))) {
                spdlog::error("main: swapchain rebuild failed — exiting");
                break;
            }

            // The pipeline must also be recreated because the colour attachment
            // format could have changed (rare but required for correctness).
            pipeline.destroy(ctx);
            if (!pipeline.init(ctx, VERT_SPV, FRAG_SPV, swap.format())) {
                spdlog::error("main: pipeline rebuild failed — exiting");
                break;
            }
        }
    }

    spdlog::info("main: window closed — shutting down");

    // ── Teardown (strict reverse construction order) ───────────────────────
    // Wait for the GPU to finish all in-flight work before touching any handle.
    renderer.waitIdle(ctx);
    renderer.destroy(ctx);
    pipeline.destroy(ctx);
    swap.destroy(ctx);
    ctx.destroy();

    glfwDestroyWindow(window);
    glfwTerminate();

    spdlog::info("main: clean shutdown");
    return EXIT_SUCCESS;
}
