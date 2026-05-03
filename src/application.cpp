/**
 * @file application.cpp
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
 * GLFW defaults to creating an OpenGL context alongside the window.  I drive
 * Vulkan myself so I must suppress this with `GLFW_CLIENT_API = GLFW_NO_API`.
 * Forgetting this hint causes GLFW to error on `glfwCreateWindow` when there
 * is no OpenGL ICD available, or silently creates an incompatible context.
 *
 * ## AppWindow resize handling
 * When the OS window is resized:
 *  1. `drawFrame()` returns false (swapchain `VK_ERROR_OUT_OF_DATE_KHR`).
 *  2. I call `renderer.waitIdle()` to ensure the GPU finishes all in-flight work.
 *  3. `swap.rebuild()` tears down and recreates the swapchain at the new size.
 *  4. I also recreate `pipeline` because the colour attachment format *could*
 *     have changed (rare, but required for correctness).
 *  5. The render loop continues from the next `drawFrame()`.
 *
 * ## Minimisation handling
 * A minimised window has a framebuffer size of 0×0.  Submitting a present with
 * zero extent is invalid.  The loop skips `drawFrame()` and calls
 * `AppWindow::waitEvents()` to avoid burning CPU while minimised.
 *
 * @note  Shader `.spv` paths are relative to the working directory.  CMake
 *        copies compiled shaders to the build output directory at build time,
 *        so running the executable from the build directory resolves correctly.
 *
 * @author Mohamed Deeq Mohamed (P2884884)
 * @date   2026-04-10
 */

#include "application.hpp"
#include "window.hpp"
#include "vulkan_context.hpp"
#include "swapchain.hpp"
#include "graphics_pipeline.hpp"
#include "frame_data.hpp"
#include "mesh.hpp"
#include "texture.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>
#include <array>
#include <chrono>
#include <cstdlib>

// =============================================================================
// Configuration constants
// =============================================================================

static constexpr int  WINDOW_WIDTH  = 1280;       ///< Initial window width in pixels.
static constexpr int  WINDOW_HEIGHT = 720;         ///< Initial window height in pixels.
static constexpr char WINDOW_TITLE[] = "FYP Vulkan Renderer"; ///< OS title bar text.

/// Path to the compiled vertex shader, relative to the working directory.
/// CMake compiles shaders/mesh.vert to shaders/mesh.vert.spv and
/// copies the result to the build output directory automatically.
static constexpr char VERT_SPV[] = "shaders/mesh.vert.spv";

/// Path to the compiled fragment shader.
static constexpr char FRAG_SPV[] = "shaders/mesh.frag.spv";
static constexpr char MODEL_PATH[] = "assets/models/viking_room.obj";
static constexpr char TEXTURE_PATH[] = "assets/textures/viking_room.png";
static constexpr glm::vec3 MODEL_OFFSET{0.0f, -0.35f, 0.0f};

// =============================================================================
// Application lifecycle
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
int Application::run()
{
    spdlog::set_level(spdlog::level::info);
    spdlog::info("=== FYP Vulkan Renderer ===");

    // ── AppWindow ────────────────────────────────────────────────────────────
    AppWindow windowSystem;
    if (!windowSystem.init(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE)) {
        return EXIT_FAILURE;
    }
    GLFWwindow* window = windowSystem.handle();

    // ── VulkanContext ─────────────────────────────────────────────────────
    // Creates: VkInstance (with validation layers) → VkSurfaceKHR →
    //          VkPhysicalDevice → VkDevice → VkQueue.
    // Everything else depends on this completing successfully.
    VulkanContext ctx;
    if (!ctx.init(window)) {
        ctx.destroy();
        windowSystem.destroy();
        return EXIT_FAILURE;
    }

    // ── SwapChain ─────────────────────────────────────────────────────────
    // I query the *framebuffer* size (physical pixels) rather than the window
    // size (logical/DPI-scaled pixels).  On high-DPI displays these differ.
    // Vulkan works in physical pixels so I must pass the framebuffer size.
    int fbWidth = 0, fbHeight = 0;
    windowSystem.framebufferSize(fbWidth, fbHeight);

    SwapChain swap;
    if (!swap.init(ctx, static_cast<uint32_t>(fbWidth),
                        static_cast<uint32_t>(fbHeight))) {
        ctx.destroy();
        windowSystem.destroy();
        return EXIT_FAILURE;
    }

    // ── Pipeline ──────────────────────────────────────────────────────────
    // I compile the mesh shaders against the current swapchain colour format
    // because Dynamic Rendering validates that the pipeline and attachment match.
    Pipeline pipeline;
    if (!pipeline.init(ctx, VERT_SPV, FRAG_SPV, swap.format())) {
        swap.destroy(ctx);
        ctx.destroy();
        windowSystem.destroy();
        return EXIT_FAILURE;
    }

    LoadedMesh loadedMesh{};
    if (!loadObjMesh(MODEL_PATH, loadedMesh)) {
        pipeline.destroy(ctx);
        swap.destroy(ctx);
        ctx.destroy();
        windowSystem.destroy();
        return EXIT_FAILURE;
    }

    Mesh mesh;
    if (!mesh.upload(ctx, loadedMesh.vertices, loadedMesh.indices)) {
        pipeline.destroy(ctx);
        swap.destroy(ctx);
        ctx.destroy();
        windowSystem.destroy();
        return EXIT_FAILURE;
    }

    LoadedTexture loadedTexture{};
    if (!loadRgba8Texture(TEXTURE_PATH, loadedTexture)) {
        mesh.destroy(ctx);
        pipeline.destroy(ctx);
        swap.destroy(ctx);
        ctx.destroy();
        windowSystem.destroy();
        return EXIT_FAILURE;
    }

    Material material;
    if (!material.init(ctx, loadedTexture, pipeline.descriptorSetLayout())) {
        mesh.destroy(ctx);
        pipeline.destroy(ctx);
        swap.destroy(ctx);
        ctx.destroy();
        windowSystem.destroy();
        return EXIT_FAILURE;
    }

    // ── Renderer ──────────────────────────────────────────────────────────
    // Creates the command pool, double-buffered command buffers, imageAvailable
    // semaphores (×MAX_FRAMES_IN_FLIGHT), renderFinished semaphores
    // (×swap.imageCount()), and inFlight fences (×MAX_FRAMES_IN_FLIGHT).
    Renderer renderer;
    if (!renderer.init(ctx, swap)) {
        material.destroy(ctx);
        mesh.destroy(ctx);
        pipeline.destroy(ctx);
        swap.destroy(ctx);
        ctx.destroy();
        windowSystem.destroy();
        return EXIT_FAILURE;
    }

    // ── ImGui ─────────────────────────────────────────────────────────────
    if (!renderer.initImGui(ctx, swap, window)) {
        renderer.destroy(ctx);
        material.destroy(ctx);
        mesh.destroy(ctx);
        pipeline.destroy(ctx);
        swap.destroy(ctx);
        ctx.destroy();
        windowSystem.destroy();
        return EXIT_FAILURE;
    }

    spdlog::info("Application: all modules initialised; entering render loop");

    // ── Frametime tracking ────────────────────────────────────────────────
    static constexpr int FRAMETIME_HISTORY = 128;
    std::array<float, FRAMETIME_HISTORY> frametimeHistory{};
    int frametimeOffset = 0;
    auto lastFrameTime  = std::chrono::high_resolution_clock::now();
    const auto appStartTime = lastFrameTime;

    // ── Render loop ───────────────────────────────────────────────────────
    while (!windowSystem.shouldClose()) {
        // Poll for OS events (keyboard, mouse, window resize/close requests).
        // Without this call the window appears frozen.
        AppWindow::pollEvents();

        // Skip rendering while minimised — zero-extent is invalid for present.
        windowSystem.framebufferSize(fbWidth, fbHeight);
        if (fbWidth == 0 || fbHeight == 0) {
            // Minimized windows report a zero-sized framebuffer.  Presenting a
            // zero extent is invalid, so I sleep until the OS sends another
            // event instead of spinning the CPU in a tight poll loop.
            AppWindow::waitEvents();
            continue;
        }

        // ── Frametime sample ──────────────────────────────────────────────
        auto   now = std::chrono::high_resolution_clock::now();
        float  dt  = std::chrono::duration<float, std::milli>(now - lastFrameTime).count();
        lastFrameTime = now;

        frametimeHistory[frametimeOffset] = dt;
        frametimeOffset = (frametimeOffset + 1) % FRAMETIME_HISTORY;

        // ── ImGui frame ───────────────────────────────────────────────────
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Performance");
        ImGui::Text("FPS:        %.1f", dt > 0.0f ? 1000.0f / dt : 0.0f);
        ImGui::Text("Frame time: %.2f ms", dt);
        ImGui::Separator();
        char overlay[32];
        snprintf(overlay, sizeof(overlay), "%.2f ms", dt);
        ImGui::PlotLines("##ft", frametimeHistory.data(), FRAMETIME_HISTORY,
                         frametimeOffset, overlay, 0.0f, 50.0f, ImVec2(0.0f, 80.0f));
        ImGui::End();

        ImGui::Render();

        // I build the camera matrices here for now because this milestone only
        // has one rotating model. A dedicated Camera module can replace this
        // once movement/input becomes part of the renderer.
        const float elapsedSeconds =
            std::chrono::duration<float>(now - appStartTime).count();
        const float aspect =
            static_cast<float>(fbWidth) / static_cast<float>(fbHeight);

        glm::mat4 model = glm::translate(glm::mat4(1.0f), MODEL_OFFSET);
        model = glm::rotate(model,
                            elapsedSeconds * 0.5f,
                            glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 2.0f),
                                     glm::vec3(0.0f, 0.0f, 0.0f),
                                     glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 projection = glm::perspective(glm::radians(60.0f),
                                                aspect,
                                                0.1f,
                                                10.0f);
        // GLM produces OpenGL-style clip coordinates by default, so I flip Y
        // to match Vulkan's framebuffer coordinate system.
        projection[1][1] *= -1.0f;
        const glm::mat4 mvp = projection * view * model;

        if (!renderer.drawFrame(ctx, swap, pipeline, mesh, material, mvp)) {
            // drawFrame returns false when the swapchain is out of date.
            // This happens on window resize or when VK_SUBOPTIMAL_KHR is returned.
            // I must wait for all in-flight GPU work to finish before
            // destroying/recreating swapchain resources — the GPU may still be
            // reading from images that are about to be destroyed.
            renderer.waitIdle(ctx);

            // Wait until the window is a non-zero size (e.g. user un-minimises).
            windowSystem.framebufferSize(fbWidth, fbHeight);
            while (fbWidth == 0 || fbHeight == 0) {
                windowSystem.framebufferSize(fbWidth, fbHeight);
                AppWindow::waitEvents(); // sleep until an OS event wakes us
            }

            // Rebuild the swapchain at the new size.
            if (!swap.rebuild(ctx, static_cast<uint32_t>(fbWidth),
                                    static_cast<uint32_t>(fbHeight))) {
                spdlog::error("Application: swapchain rebuild failed; exiting");
                break;
            }

            // renderFinished semaphores are indexed by swapchain image index,
            // so their count must be rebuilt if the driver gives me a different
            // number of images after resize.
            if (!renderer.recreateSwapchainSync(ctx, swap)) {
                spdlog::error("Application: swapchain sync rebuild failed; exiting");
                break;
            }

            // I must also recreate the pipeline because the colour attachment
            // format *could* have changed after a swapchain rebuild.  In practice
            // the format rarely changes, but the spec does not guarantee it stays
            // the same, so rebuilding is the correct approach.
            material.destroy(ctx);
            pipeline.destroy(ctx);
            if (!pipeline.init(ctx, VERT_SPV, FRAG_SPV, swap.format())) {
                spdlog::error("Application: pipeline rebuild failed; exiting");
                break;
            }
            if (!material.init(ctx, loadedTexture, pipeline.descriptorSetLayout())) {
                spdlog::error("Application: material rebuild failed; exiting");
                break;
            }

            // ImGui owns a Vulkan backend pipeline that is configured with the
            // swapchain image count and colour format.  Recreate it after the
            // swapchain/pipeline rebuild so the debug overlay targets the same
            // attachment format as the rest of the frame.
            if (!renderer.recreateImGui(ctx, swap, window)) {
                spdlog::error("Application: ImGui rebuild failed; exiting");
                break;
            }
        }
    }

    spdlog::info("Application: window closed; shutting down");

    // ── Teardown (strict reverse construction order) ───────────────────────
    // I wait for the GPU to drain all in-flight frames before touching any handle.
    // Destroying a resource the GPU is still reading is undefined behaviour.
    renderer.waitIdle(ctx);

    // Destroy in reverse init order: ImGui → Renderer → Pipeline → SwapChain → Context.
    renderer.shutdownImGui(ctx);
    renderer.destroy(ctx);
    material.destroy(ctx);
    mesh.destroy(ctx);
    pipeline.destroy(ctx);
    swap.destroy(ctx);
    ctx.destroy();

    windowSystem.destroy();

    spdlog::info("Application: clean shutdown");
    return EXIT_SUCCESS;
}
