/**
 * @file main.cpp
 * @brief Application entry point — window creation, module init sequence and render loop.
 *
 * ## High-level flow
 * ```
 * glfwInit()
 *   └─ Create window (GLFW_NO_API — no OpenGL context)
 *       └─ VulkanContext::init()     → instance, GPU, device, queue
 *           └─ SwapChain::init()     → swapchain images & views
 *               └─ Pipeline::init()  → SPIR-V shaders & graphics pipeline
 *                   └─ Renderer::init() → cmd buffers, semaphores, fences
 *                       └─ Render loop (drawFrame each tick)
 *                           └─ Teardown (reverse order)
 * ```
 *
 * ## Why GLFW_NO_API?
 * GLFW defaults to creating an OpenGL context alongside the window.  We drive
 * Vulkan ourselves so we must suppress this with `GLFW_CLIENT_API = GLFW_NO_API`.
 * Forgetting this hint causes GLFW to error on `glfwCreateWindow` when there
 * is no OpenGL ICD available, or silently creates an incompatible context.
 *
 * ## Window resize handling
 * When the OS window is resized:
 *  1. `drawFrame()` returns false (swapchain `VK_ERROR_OUT_OF_DATE_KHR`).
 *  2. `renderer.waitIdle()` ensures the GPU finishes all in-flight work.
 *  3. `swap.rebuild()` tears down and recreates the swapchain at the new size.
 *  4. `pipeline` is also recreated because the colour attachment format *could*
 *     have changed (rare, but required for correctness).
 *  5. The render loop continues from the next `drawFrame()`.
 *
 * ## Minimisation handling
 * A minimised window has a framebuffer size of 0×0.  Submitting a present with
 * zero extent is invalid.  The loop skips `drawFrame()` and calls
 * `glfwWaitEvents()` to avoid burning CPU while minimised.
 *
 * @note  Shader `.spv` paths are relative to the working directory.  CMake
 *        copies compiled shaders to the build output directory at build time,
 *        so running the executable from the build directory resolves correctly.
 *
 * @author Mohamed Deeq Mohamed (P2884884)
 * @date   2026-04-10
 */

#define GLFW_INCLUDE_VULKAN   // Makes glfw3.h pull in vulkan.h as well.
#include <GLFW/glfw3.h>

#include "VulkanContext.h"
#include "SwapChain.h"
#include "Pipeline.h"
#include "Renderer.h"

#include <spdlog/spdlog.h>
#include <cstdlib>

// =============================================================================
// Configuration constants
// =============================================================================

static constexpr int  WINDOW_WIDTH  = 1280;       ///< Initial window width in pixels.
static constexpr int  WINDOW_HEIGHT = 720;         ///< Initial window height in pixels.
static constexpr char WINDOW_TITLE[] = "FYP Vulkan Renderer — M1 Triangle"; ///< OS title bar text.

/// Path to the compiled vertex shader, relative to the working directory.
/// CMake compiles shaders/triangle.vert → shaders/triangle.vert.spv and
/// copies the result to the build output directory automatically.
static constexpr char VERT_SPV[] = "shaders/triangle.vert.spv";

/// Path to the compiled fragment shader.
static constexpr char FRAG_SPV[] = "shaders/triangle.frag.spv";

// =============================================================================
// main()
// =============================================================================

/**
 * @brief Application entry point.
 *
 * Follows a strict init → loop → teardown pattern.  Every early-exit path
 * cleans up the resources that had been successfully created before the failure.
 *
 * @return EXIT_SUCCESS (0) on a clean, user-initiated shutdown.
 * @return EXIT_FAILURE (1) if any initialisation step fails.
 */
int main()
{
    spdlog::set_level(spdlog::level::info);
    spdlog::info("=== FYP Vulkan Renderer — Milestone 1 ===");

    // ── GLFW init ─────────────────────────────────────────────────────────
    // glfwInit() loads the GLFW library and initialises the event system.
    // It must be called before any other GLFW function.
    if (!glfwInit()) {
        spdlog::error("main: glfwInit failed");
        return EXIT_FAILURE;
    }

    // GLFW_NO_API: do not create an OpenGL context.  Vulkan manages its own
    // rendering — we don't want GLFW to create an incompatible GL context.
    // GLFW_RESIZABLE: allow the user to drag-resize the window; we handle the
    // resulting swapchain rebuild in the render loop below.
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
    // Creates: VkInstance (with validation layers) → VkSurfaceKHR →
    //          VkPhysicalDevice → VkDevice → VkQueue.
    // Everything else depends on this completing successfully.
    VulkanContext ctx;
    if (!ctx.init(window)) {
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    // ── SwapChain ─────────────────────────────────────────────────────────
    // We query the *framebuffer* size (physical pixels) rather than the window
    // size (logical/DPI-scaled pixels).  On high-DPI displays these differ.
    // Vulkan works in physical pixels so we must pass the framebuffer size.
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
    // Loads the triangle SPIR-V shaders and compiles the graphics pipeline.
    // The swapchain colour format is passed so the pipeline's
    // VkPipelineRenderingCreateInfo matches the actual attachment format.
    Pipeline pipeline;
    if (!pipeline.init(ctx, VERT_SPV, FRAG_SPV, swap.format())) {
        swap.destroy(ctx);
        ctx.destroy();
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    // ── Renderer ──────────────────────────────────────────────────────────
    // Creates the command pool, double-buffered command buffers, imageAvailable
    // semaphores (×MAX_FRAMES_IN_FLIGHT), renderFinished semaphores
    // (×swap.imageCount()), and inFlight fences (×MAX_FRAMES_IN_FLIGHT).
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
        // Poll for OS events (keyboard, mouse, window resize/close requests).
        // Without this call the window appears frozen.
        glfwPollEvents();

        // Skip rendering while minimised.  A zero-size framebuffer means the
        // swapchain extent would be 0×0, which is an invalid argument to the
        // Vulkan present call.  We wait for an event (glfwWaitEvents) instead
        // of spinning to avoid burning CPU while the window is minimised.
        glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
        if (fbWidth == 0 || fbHeight == 0) {
            continue;
        }

        if (!renderer.drawFrame(ctx, swap, pipeline)) {
            // drawFrame returns false when the swapchain is out of date.
            // This happens on window resize or when VK_SUBOPTIMAL_KHR is returned.
            // We must wait for all in-flight GPU work to finish before
            // destroying/recreating swapchain resources — the GPU may still be
            // reading from images that are about to be destroyed.
            renderer.waitIdle(ctx);

            // Wait until the window is a non-zero size (e.g. user un-minimises).
            glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
            while (fbWidth == 0 || fbHeight == 0) {
                glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
                glfwWaitEvents(); // sleep until an OS event wakes us
            }

            // Rebuild the swapchain at the new size.
            if (!swap.rebuild(ctx, static_cast<uint32_t>(fbWidth),
                                    static_cast<uint32_t>(fbHeight))) {
                spdlog::error("main: swapchain rebuild failed — exiting");
                break;
            }

            // The pipeline must also be recreated because the colour attachment
            // format *could* have changed after a swapchain rebuild.  In practice
            // the format rarely changes, but the spec does not guarantee it stays
            // the same, so rebuilding is the correct approach.
            pipeline.destroy(ctx);
            if (!pipeline.init(ctx, VERT_SPV, FRAG_SPV, swap.format())) {
                spdlog::error("main: pipeline rebuild failed — exiting");
                break;
            }
        }
    }

    spdlog::info("main: window closed — shutting down");

    // ── Teardown (strict reverse construction order) ───────────────────────
    // Wait for the GPU to drain all in-flight frames before touching any handle.
    // Destroying a resource the GPU is still reading is undefined behaviour.
    renderer.waitIdle(ctx);

    // Destroy in reverse init order: Renderer → Pipeline → SwapChain → Context.
    renderer.destroy(ctx);
    pipeline.destroy(ctx);
    swap.destroy(ctx);
    ctx.destroy();

    glfwDestroyWindow(window);
    glfwTerminate();

    spdlog::info("main: clean shutdown");
    return EXIT_SUCCESS;
}
