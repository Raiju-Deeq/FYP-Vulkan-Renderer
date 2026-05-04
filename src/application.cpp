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
#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

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

/// Small scene-space offset used to keep the Viking model centred in the view.
/// I keep this as a named constant so I can tune the placement without touching
/// the OBJ import transform or camera setup.
static constexpr glm::vec3 MODEL_OFFSET{0.0f, -0.35f, 0.0f};

static constexpr float CAMERA_START_DISTANCE = 2.0f; ///< Initial camera distance from the model.
static constexpr float CAMERA_MIN_DISTANCE = 0.75f;  ///< Closest scroll-wheel zoom distance.
static constexpr float CAMERA_MAX_DISTANCE = 8.0f;   ///< Furthest scroll-wheel zoom distance.
static constexpr float CAMERA_ZOOM_STEP = 0.25f;     ///< Distance changed per wheel notch.

namespace
{
/**
 * @enum AssetBrowserTarget
 * @brief Tells the ImGui asset browser which file type I am looking for.
 */
enum class AssetBrowserTarget
{
    Model,   ///< OBJ mesh file.
    Texture  ///< Image file for the material albedo texture.
};

/**
 * @struct AssetSelection
 * @brief Result returned by the ImGui asset browser after the user picks a file.
 */
struct AssetSelection
{
    AssetBrowserTarget target;     ///< Whether this selection is a model or texture.
    std::filesystem::path path;    ///< Filesystem path chosen in the browser.
};

/**
 * @struct AssetBrowserState
 * @brief Persistent UI state for the small in-app file browser.
 */
struct AssetBrowserState
{
    bool open = false; ///< True while the modal file browser should be visible.
    AssetBrowserTarget target = AssetBrowserTarget::Model; ///< Current browser mode.
    std::filesystem::path currentDirectory{"assets"}; ///< Directory being listed.
};

/**
 * @struct MeshPipelines
 * @brief The four rasterizer variants needed by the debug checkboxes.
 *
 * Vulkan bakes polygon mode and cull mode into the graphics pipeline, so the
 * renderer cannot flip wireframe/culling as simple dynamic state.  I create the
 * four useful combinations once and select one each frame from the ImGui flags.
 */
struct MeshPipelines
{
    Pipeline solid;            ///< Filled triangles, double-sided.
    Pipeline solidCulled;      ///< Filled triangles, back-face culled.
    Pipeline wireframe;        ///< Wireframe triangles, double-sided.
    Pipeline wireframeCulled;  ///< Wireframe triangles, back-face culled.
};

/**
 * @brief Checks whether a path has one of the browser's allowed extensions.
 * @param path File path to test.
 * @param allowedExtensions Lower-case extensions including the dot.
 * @return true when the path extension matches an allowed extension.
 */
bool hasExtension(const std::filesystem::path& path,
                  const std::vector<std::string>& allowedExtensions)
{
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return std::find(allowedExtensions.begin(), allowedExtensions.end(), extension) !=
           allowedExtensions.end();
}

/**
 * @brief Returns the file extensions shown for the current browser target.
 * @param target Browser mode: model or texture.
 * @return Allowed lower-case extensions for that target.
 */
std::vector<std::string> allowedExtensions(AssetBrowserTarget target)
{
    if (target == AssetBrowserTarget::Model) {
        return {".obj"};
    }
    return {".png", ".jpg", ".jpeg", ".bmp", ".tga", ".ppm"};
}

/**
 * @brief Returns the modal title for the current browser target.
 * @param target Browser mode.
 * @return Human-readable modal title.
 */
const char* assetBrowserTitle(AssetBrowserTarget target)
{
    return target == AssetBrowserTarget::Model ? "Find model" : "Find texture";
}

/**
 * @brief Opens the asset browser beside the currently loaded asset.
 * @param browser Browser state to update.
 * @param target Whether the user is looking for a model or texture.
 * @param currentAssetPath Current asset path, used to pick a useful start folder.
 */
void openAssetBrowser(AssetBrowserState& browser,
                      AssetBrowserTarget target,
                      const std::filesystem::path& currentAssetPath)
{
    browser.open = true;
    browser.target = target;

    const std::filesystem::path parent = currentAssetPath.parent_path();
    if (!parent.empty() && std::filesystem::exists(parent)) {
        browser.currentDirectory = parent;
    }
}

/**
 * @brief Draws the in-app modal file browser.
 *
 * The browser intentionally stays simple: it lists directories first, filters
 * files by extension, and returns one selected path.  It avoids adding a native
 * file-dialog dependency while still letting me swap assets during a demo.
 *
 * @param browser Persistent browser UI state.
 * @return A selected asset path when the user clicks a valid file.
 */
std::optional<AssetSelection> drawAssetBrowser(AssetBrowserState& browser)
{
    if (!browser.open) {
        return std::nullopt;
    }

    ImGui::OpenPopup("Asset Browser");
    std::optional<AssetSelection> selection;

    ImGui::SetNextWindowSize(ImVec2(640.0f, 420.0f), ImGuiCond_FirstUseEver);
    if (ImGui::BeginPopupModal("Asset Browser", &browser.open)) {
        ImGui::Text("%s", assetBrowserTitle(browser.target));
        ImGui::TextWrapped("%s", browser.currentDirectory.string().c_str());
        ImGui::Separator();

        if (ImGui::Button("Up")) {
            const std::filesystem::path parent = browser.currentDirectory.parent_path();
            if (!parent.empty()) {
                browser.currentDirectory = parent;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            browser.open = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::Separator();
        ImGui::BeginChild("asset-files", ImVec2(0.0f, 0.0f), true);

        std::vector<std::filesystem::directory_entry> entries;
        std::error_code error;
        for (const auto& entry : std::filesystem::directory_iterator(browser.currentDirectory, error)) {
            entries.push_back(entry);
        }

        std::sort(entries.begin(), entries.end(),
                  [](const auto& a, const auto& b) {
                      if (a.is_directory() != b.is_directory()) {
                          return a.is_directory();
                      }
                      return a.path().filename().string() < b.path().filename().string();
                  });

        const std::vector<std::string> extensions = allowedExtensions(browser.target);
        for (const auto& entry : entries) {
            const std::filesystem::path path = entry.path();
            const std::string label = entry.is_directory()
                ? "[dir] " + path.filename().string()
                : path.filename().string();

            if (entry.is_directory()) {
                if (ImGui::Selectable(label.c_str())) {
                    browser.currentDirectory = path;
                }
                continue;
            }

            if (!hasExtension(path, extensions)) {
                continue;
            }

            if (ImGui::Selectable(label.c_str())) {
                selection = AssetSelection{browser.target, path};
                browser.open = false;
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::EndChild();
        ImGui::EndPopup();
    }

    return selection;
}

/**
 * @brief Destroys all mesh pipeline variants.
 * @param ctx Vulkan context that owns the VkDevice.
 * @param pipelines Pipeline bundle to destroy.
 */
void destroyMeshPipelines(const VulkanContext& ctx, MeshPipelines& pipelines)
{
    pipelines.wireframeCulled.destroy(ctx);
    pipelines.wireframe.destroy(ctx);
    pipelines.solidCulled.destroy(ctx);
    pipelines.solid.destroy(ctx);
}

/**
 * @brief Builds all mesh pipeline variants used by the debug UI.
 * @param ctx Vulkan context.
 * @param colourFormat Swapchain colour attachment format.
 * @param depthFormat Swapchain depth attachment format.
 * @param pipelines Receives the four pipeline variants.
 * @return true if every pipeline was created successfully.
 */
bool initMeshPipelines(const VulkanContext& ctx,
                       VkFormat colourFormat,
                       VkFormat depthFormat,
                       MeshPipelines& pipelines)
{
    if (!pipelines.solid.init(ctx, VERT_SPV, FRAG_SPV, colourFormat, depthFormat,
                              VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE)) {
        return false;
    }

    if (!pipelines.solidCulled.init(ctx, VERT_SPV, FRAG_SPV, colourFormat, depthFormat,
                                    VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT)) {
        destroyMeshPipelines(ctx, pipelines);
        return false;
    }

    if (!pipelines.wireframe.init(ctx, VERT_SPV, FRAG_SPV, colourFormat, depthFormat,
                                  VK_POLYGON_MODE_LINE, VK_CULL_MODE_NONE)) {
        destroyMeshPipelines(ctx, pipelines);
        return false;
    }

    if (!pipelines.wireframeCulled.init(ctx, VERT_SPV, FRAG_SPV, colourFormat, depthFormat,
                                        VK_POLYGON_MODE_LINE, VK_CULL_MODE_BACK_BIT)) {
        destroyMeshPipelines(ctx, pipelines);
        return false;
    }

    return true;
}

/**
 * @brief Selects the active mesh pipeline for this frame.
 * @param pipelines Available pipeline variants.
 * @param showWireframe true for line rendering, false for filled triangles.
 * @param useBackfaceCulling true to hide back-facing triangles.
 * @return Pipeline matching the current debug checkbox state.
 */
const Pipeline& selectMeshPipeline(const MeshPipelines& pipelines,
                                   bool showWireframe,
                                   bool useBackfaceCulling)
{
    if (showWireframe) {
        return useBackfaceCulling ? pipelines.wireframeCulled
                                  : pipelines.wireframe;
    }

    return useBackfaceCulling ? pipelines.solidCulled
                              : pipelines.solid;
}
} // namespace

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

    // ── Pipelines ─────────────────────────────────────────────────────────
    // I keep fill/line and culled/unculled variants because both wireframe and
    // face culling are fixed rasterizer state in Vulkan.
    MeshPipelines pipelines;
    if (!initMeshPipelines(ctx, swap.format(), swap.depthFormat(), pipelines)) {
        swap.destroy(ctx);
        ctx.destroy();
        windowSystem.destroy();
        return EXIT_FAILURE;
    }

    LoadedMesh initialMeshData{};
    if (!loadObjMesh(MODEL_PATH, initialMeshData)) {
        destroyMeshPipelines(ctx, pipelines);
        swap.destroy(ctx);
        ctx.destroy();
        windowSystem.destroy();
        return EXIT_FAILURE;
    }

    Mesh mesh;
    if (!mesh.upload(ctx, initialMeshData.vertices, initialMeshData.indices)) {
        destroyMeshPipelines(ctx, pipelines);
        swap.destroy(ctx);
        ctx.destroy();
        windowSystem.destroy();
        return EXIT_FAILURE;
    }

    LoadedTexture loadedTexture{};
    if (!loadRgba8Texture(TEXTURE_PATH, loadedTexture)) {
        mesh.destroy(ctx);
        destroyMeshPipelines(ctx, pipelines);
        swap.destroy(ctx);
        ctx.destroy();
        windowSystem.destroy();
        return EXIT_FAILURE;
    }

    Material material;
    if (!material.init(ctx, loadedTexture, pipelines.solid.descriptorSetLayout())) {
        mesh.destroy(ctx);
        destroyMeshPipelines(ctx, pipelines);
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
        destroyMeshPipelines(ctx, pipelines);
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
        destroyMeshPipelines(ctx, pipelines);
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
    std::filesystem::path currentModelPath{MODEL_PATH};
    std::filesystem::path currentTexturePath{TEXTURE_PATH};
    AssetBrowserState assetBrowser{};

    // S2 debug controls. These are independent on purpose:
    // - Wireframe swaps between the solid and line-mode pipelines.
    // - Backface culling hides inside faces for closed meshes.
    // - Normals view changes the fragment shader output through a push constant.
    bool showWireframe = false;
    bool useBackfaceCulling = true;
    bool showNormals = false;
    float cameraDistance = CAMERA_START_DISTANCE;

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

        // Mouse-wheel zoom. I read the wheel value from ImGui because the
        // existing ImGui GLFW backend already owns the GLFW scroll callback.
        // When the mouse is over an ImGui control, I leave the camera alone so
        // file browsing and panel interaction do not accidentally zoom.
        const ImGuiIO& io = ImGui::GetIO();
        if (!io.WantCaptureMouse && io.MouseWheel != 0.0f) {
            cameraDistance = std::clamp(cameraDistance - io.MouseWheel * CAMERA_ZOOM_STEP,
                                        CAMERA_MIN_DISTANCE,
                                        CAMERA_MAX_DISTANCE);
        }

        // Keep the debug UI docked to the left side of the renderer window.
        // This makes the S2 controls feel like a fixed inspection panel rather
        // than a floating popup that can cover the model.
        constexpr float DEBUG_PANEL_WIDTH = 260.0f;
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(DEBUG_PANEL_WIDTH, static_cast<float>(fbHeight)),
                                 ImGuiCond_Always);

        const ImGuiWindowFlags panelFlags =
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoSavedSettings;

        ImGui::Begin("Performance", nullptr, panelFlags);
        ImGui::Text("FPS:        %.1f", dt > 0.0f ? 1000.0f / dt : 0.0f);
        ImGui::Text("Frame time: %.2f ms", dt);
        ImGui::Text("Camera:     %.2f", cameraDistance);
        ImGui::Separator();
        char overlay[32];
        snprintf(overlay, sizeof(overlay), "%.2f ms", dt);
        ImGui::PlotLines("##ft", frametimeHistory.data(), FRAMETIME_HISTORY,
                         frametimeOffset, overlay, 0.0f, 50.0f, ImVec2(0.0f, 80.0f));
        ImGui::Separator();

        // S2: small mesh inspection controls for the report/demo.
        // Backface culling hides inside faces; disable it for double-sided assets.
        // Wireframe shows triangle topology; normals view shows imported normals.
        ImGui::Checkbox("Backface culling", &useBackfaceCulling);
        ImGui::Checkbox("Wireframe", &showWireframe);
        ImGui::Checkbox("Normals view", &showNormals);
        ImGui::Separator();

        ImGui::Text("Assets");
        if (ImGui::Button("Find model")) {
            openAssetBrowser(assetBrowser, AssetBrowserTarget::Model, currentModelPath);
        }
        ImGui::TextWrapped("%s", currentModelPath.string().c_str());

        if (ImGui::Button("Find texture")) {
            openAssetBrowser(assetBrowser, AssetBrowserTarget::Texture, currentTexturePath);
        }
        ImGui::TextWrapped("%s", currentTexturePath.string().c_str());
        ImGui::End();

        const std::optional<AssetSelection> selectedAsset = drawAssetBrowser(assetBrowser);

        ImGui::Render();

        if (selectedAsset.has_value()) {
            const std::filesystem::path& selectedPath = selectedAsset->path;

            if (selectedAsset->target == AssetBrowserTarget::Model) {
                LoadedMesh replacementMeshData{};
                if (loadObjMesh(selectedPath.string(), replacementMeshData)) {
                    renderer.waitIdle(ctx);
                    if (mesh.upload(ctx, replacementMeshData.vertices, replacementMeshData.indices)) {
                        currentModelPath = selectedPath;
                    }
                }
            } else {
                LoadedTexture replacementTexture{};
                if (loadRgba8Texture(selectedPath.string(), replacementTexture)) {
                    renderer.waitIdle(ctx);
                    if (material.init(ctx, replacementTexture, pipelines.solid.descriptorSetLayout())) {
                        loadedTexture = std::move(replacementTexture);
                        currentTexturePath = selectedPath;
                    }
                }
            }
        }

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
        glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, cameraDistance),
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

        // The debug checkboxes map to pipeline state for wireframe/culling and
        // shader state for normal visualisation.
        const Pipeline& activePipeline =
            selectMeshPipeline(pipelines, showWireframe, useBackfaceCulling);
        const DebugViewMode debugViewMode = showNormals ? DebugViewMode::Normals
                                                        : DebugViewMode::Lit;

        if (!renderer.drawFrame(ctx, swap, activePipeline, mesh, material, mvp, debugViewMode)) {
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

            // I must also recreate the pipelines because the colour attachment
            // format *could* have changed after a swapchain rebuild.  In practice
            // the format rarely changes, but the spec does not guarantee it stays
            // the same, so rebuilding is the correct approach.
            material.destroy(ctx);
            destroyMeshPipelines(ctx, pipelines);
            if (!initMeshPipelines(ctx, swap.format(), swap.depthFormat(), pipelines)) {
                spdlog::error("Application: mesh pipeline rebuild failed; exiting");
                break;
            }
            if (!material.init(ctx, loadedTexture, pipelines.solid.descriptorSetLayout())) {
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
    destroyMeshPipelines(ctx, pipelines);
    swap.destroy(ctx);
    ctx.destroy();

    windowSystem.destroy();

    spdlog::info("Application: clean shutdown");
    return EXIT_SUCCESS;
}
