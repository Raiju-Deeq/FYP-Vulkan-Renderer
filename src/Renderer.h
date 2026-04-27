/**
 * @file Renderer.h
 * @brief Per-frame command recording, GPU synchronisation and present loop.
 *
 * ## Renderer's role in the architecture
 * Renderer is the "engine room" — it owns all the GPU objects that drive the
 * per-frame render loop.  SwapChain provides the images, Pipeline provides
 * the shader program, and Renderer does the work of:
 *  1. Waiting until the GPU is ready for new work.
 *  2. Acquiring the next swapchain image.
 *  3. Recording draw commands into a command buffer.
 *  4. Submitting those commands to the GPU.
 *  5. Presenting the finished image to the display.
 *
 * ## Double-buffering
 * `MAX_FRAMES_IN_FLIGHT = 2` means the CPU can be recording frame N+1 while
 * the GPU is still executing frame N.  Each "frame slot" has its own command
 * buffer, imageAvailable semaphore and in-flight fence so the two frames do
 * not interfere with each other.
 *
 * ## Synchronisation primitives
 * Vulkan has two binary sync types.  Knowing which to use where is critical:
 *
 * | Primitive        | Who waits?  | Who signals? | Use case here                 |
 * |------------------|-------------|--------------|-------------------------------|
 * | `VkSemaphore`    | GPU only    | GPU only     | GPU-GPU ordering (acquire→draw, draw→present) |
 * | `VkFence`        | CPU (`vkWaitForFences`) | GPU | CPU blocks until GPU frame is done |
 *
 * ## Semaphore indexing (important)
 * `imageAvailableSemaphores` are indexed by **frame slot** (`m_currentFrame`).
 * `renderFinishedSemaphores` are indexed by **swapchain image index** (`imageIndex`).
 *
 * The reason for the split: if both were indexed by frame slot, a semaphore
 * could be reused while the presentation engine still holds it — this fires
 * `VUID-vkQueueSubmit-pSignalSemaphores-00067`.  Indexing `renderFinished` by
 * `imageIndex` is safe because the swapchain only re-issues image N after its
 * previous presentation is complete, guaranteeing the semaphore is free.
 *
 * @author Mohamed Deeq Mohamed (P2884884)
 * @date   2026-03-27
 */

#ifndef FYP_VULKAN_RENDERER_RENDERER_H
#define FYP_VULKAN_RENDERER_RENDERER_H

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <cstdint>
#include <vector>

class VulkanContext;
class SwapChain;
class Pipeline;
class Mesh;
class Material;
struct GLFWwindow;

/// @brief Number of frames the CPU can be ahead of the GPU.
/// 2 = double-buffering: GPU works on frame N while CPU records frame N+1.
static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

/**
 * @class Renderer
 * @brief I own per-frame GPU resources and drive the main render loop.
 *
 * ## Ownership model
 * - **I own:** `VkCommandPool`, `VkCommandBuffer[]`, `VkSemaphore[]` (both
 *   arrays), `VkFence[]`.
 * - **I do NOT own:** swapchain images, pipeline, VulkanContext handles.
 *
 * ## Dependency
 * I must be initialised after both VulkanContext and SwapChain.  I read
 * `swap.imageCount()` to size the renderFinished semaphore array.
 *
 * ## Usage
 * @code
 *   Renderer renderer;
 *   renderer.init(ctx, swap);
 *   while (!done) {
 *       if (!renderer.drawFrame(ctx, swap, pipeline)) {
 *           renderer.waitIdle(ctx);
 *           swap.rebuild(ctx, w, h);
 *           pipeline.destroy(ctx);
 *           pipeline.init(ctx, ...);
 *       }
 *   }
 *   renderer.waitIdle(ctx);
 *   renderer.destroy(ctx);
 * @endcode
 */
class Renderer
{
public:
    // =========================================================================
    // Lifecycle
    // =========================================================================

    /**
     * @brief Creates the command pool, command buffers and all sync objects.
     *
     * **Command pool:**
     * Created with `VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT` so
     * individual command buffers can be reset and re-recorded each frame
     * without resetting the whole pool.
     *
     * **imageAvailable semaphores** (`MAX_FRAMES_IN_FLIGHT` of them):
     * Passed to `vkAcquireNextImageKHR`.  The driver signals these when a
     * swapchain image finishes being presented and is free to write into.
     *
     * **renderFinished semaphores** (`swap.imageCount()` of them):
     * Signalled at queue submit; waited on at present.  One per swapchain
     * image (not per frame slot) to avoid semaphore reuse while the
     * presentation engine still holds a reference.
     *
     * **inFlight fences** (`MAX_FRAMES_IN_FLIGHT` of them):
     * Created in the *signalled* state so the very first `vkWaitForFences`
     * call on frame 0 returns immediately without blocking.
     *
     * @param  ctx   Fully initialised VulkanContext.
     * @param  swap  Fully initialised SwapChain (imageCount() is read here).
     * @return true  on success.
     * @return false if any Vulkan object creation fails.
     */
    bool init(const VulkanContext& ctx, const SwapChain& swap);

    /**
     * @brief Initialises the Dear ImGui context and both GLFW + Vulkan backends.
     *
     * Creates a dedicated VkDescriptorPool for ImGui's font atlas, then
     * initialises `ImGui_ImplGlfw` and `ImGui_ImplVulkan` with dynamic
     * rendering so no VkRenderPass is required.  Font textures are uploaded
     * immediately.
     *
     * Call after init() and before the render loop.  Call shutdownImGui()
     * before destroy().
     *
     * @param ctx    Fully initialised VulkanContext.
     * @param swap   Fully initialised SwapChain (format and imageCount are read).
     * @param window The GLFW window used for input event routing.
     * @return true  on success.
     * @return false if descriptor pool creation or backend init fails.
     */
    bool initImGui(const VulkanContext& ctx, const SwapChain& swap, GLFWwindow* window);

    /**
     * @brief Rebuilds the ImGui Vulkan backend after a swapchain recreation.
     *
     * ImGui creates its own small graphics pipeline internally.  Because this
     * project uses Dynamic Rendering, that pipeline is configured with the
     * swapchain colour format and image count at init time.  If the swapchain is
     * rebuilt, I rebuild the backend too so the overlay stays in step with the
     * current swapchain.
     *
     * @param  ctx    Fully initialised VulkanContext.
     * @param  swap   Newly rebuilt SwapChain.
     * @param  window The GLFW window used for ImGui input routing.
     * @return true   if ImGui is ready to render into the new swapchain.
     */
    bool recreateImGui(const VulkanContext& ctx, const SwapChain& swap, GLFWwindow* window);

    /**
     * @brief Shuts down both ImGui backends, destroys the context, and frees
     *        the ImGui descriptor pool.
     *
     * Must be called while the VkDevice is still valid (i.e. before destroy()
     * and before ctx.destroy()).  Always call waitIdle() first.
     *
     * @param ctx  The same VulkanContext passed to initImGui().
     */
    void shutdownImGui(const VulkanContext& ctx);

    /**
     * @brief Recreates sync objects whose count depends on the swapchain image count.
     *
     * Most Renderer objects are tied to frame slots (`MAX_FRAMES_IN_FLIGHT`) and
     * survive a resize.  `m_renderFinishedSemaphores` is different: it is indexed
     * by swapchain image index, so its length must match the current swapchain.
     *
     * I call this immediately after `SwapChain::rebuild()`, while the device is
     * idle, before the next frame can acquire an image from the new swapchain.
     *
     * @param  ctx   Fully initialised VulkanContext.
     * @param  swap  Newly rebuilt SwapChain.
     * @return true  if the semaphore array now matches `swap.imageCount()`.
     */
    bool recreateSwapchainSync(const VulkanContext& ctx, const SwapChain& swap);

    /**
     * @brief Destroys all owned sync objects, command buffers and the pool.
     *
     * @note  Call `waitIdle()` before `destroy()` to ensure the GPU has
     *        finished all in-flight work.  Destroying a semaphore or fence
     *        while it is pending on the GPU is undefined behaviour.
     *
     * @param ctx  The same VulkanContext passed to init().
     */
    void destroy(const VulkanContext& ctx);

    // =========================================================================
    // Per-frame
    // =========================================================================

    /**
     * @brief Records and submits one frame, then queues it for presentation.
     *
     * **Full 13-step frame sequence:**
     *
     *  1. `vkWaitForFences` — block the CPU until the GPU finishes the
     *     previous use of this frame slot.
     *  2. `acquireNextImage` — get the next swapchain image index; signal
     *     `imageAvailableSemaphores[currentFrame]` when the image is free.
     *  3. `vkResetFences` — reset the fence so it can be signalled again
     *     at submit time.  Reset *after* a successful acquire (not before).
     *  4. `vkResetCommandBuffer` + `vkBeginCommandBuffer` — clear the
     *     previous frame's commands and start recording new ones.
     *  5. **Barrier: UNDEFINED → COLOR_ATTACHMENT_OPTIMAL** — discard the
     *     old image contents and transition the layout so the GPU can write
     *     colour data into it.
     *  6. `vkCmdBeginRendering` — start the Dynamic Rendering scope; declare
     *     the colour attachment and its load/store operations.
     *  7. `vkCmdSetViewport` + `vkCmdSetScissor` — set dynamic state to
     *     match the current swapchain extent.
     *  8. `vkCmdBindPipeline` + `vkCmdDraw` — bind the pipeline and emit
     *     the draw call (3 vertices, 1 instance).
     *  9. `vkCmdEndRendering` — close the Dynamic Rendering scope.
     * 10. **Barrier: COLOR_ATTACHMENT_OPTIMAL → PRESENT_SRC_KHR** — transition
     *     the image so the presentation engine can read it for display.
     * 11. `vkEndCommandBuffer` — finalise the recording.
     * 12. `vkQueueSubmit` — wait on `imageAvailable`, signal `renderFinished`
     *     and the in-flight fence.
     * 13. `vkQueuePresentKHR` — wait on `renderFinished`, then flip.
     *
     * @param  ctx      Fully initialised VulkanContext.
     * @param  swap     Initialised SwapChain (images may be read this frame).
     * @param  pipeline Compiled graphics pipeline.
     * @return true     Frame submitted and queued for presentation successfully.
     * @return false    Swapchain is out of date (`VK_ERROR_OUT_OF_DATE_KHR` or
     *                  `VK_SUBOPTIMAL_KHR`) — caller must rebuild the swapchain
     *                  and pipeline before the next frame.
     */
    bool drawFrame(const VulkanContext& ctx,
                   SwapChain&           swap,
                   const Pipeline&      pipeline,
                   const Mesh&          mesh,
                   const Material&      material,
                   const glm::mat4&     mvp);

    /**
     * @brief Blocks until all queued GPU work has completed.
     *
     * Wraps `vkDeviceWaitIdle`.  Must be called before destroying any
     * resource that an in-flight command buffer might still reference:
     *  - Before `destroy()`.
     *  - Before rebuilding the swapchain on window resize.
     *  - Before rebuilding the pipeline on resize.
     *
     * @param ctx  Initialised VulkanContext.
     */
    void waitIdle(const VulkanContext& ctx);

private:
    // =========================================================================
    // Owned handles
    // =========================================================================

    /// Pool from which all command buffers are allocated.
    /// `RESET_COMMAND_BUFFER_BIT` allows per-buffer reset each frame.
    VkCommandPool m_commandPool = VK_NULL_HANDLE;

    /// One primary command buffer per in-flight frame slot.
    /// Reset and re-recorded every frame.
    VkCommandBuffer m_commandBuffers[MAX_FRAMES_IN_FLIGHT] = {};

    /// Signalled by the driver when swapchain image [i] is free to write into.
    /// Indexed by frame slot (m_currentFrame); fence ensures reuse is safe.
    VkSemaphore m_imageAvailableSemaphores[MAX_FRAMES_IN_FLIGHT] = {};

    /// Signalled at submit when rendering to image [i] is complete.
    /// **Indexed by swapchain imageIndex** — one per image, not per frame slot.
    /// See the file-level doc for the rationale behind this split indexing.
    std::vector<VkSemaphore> m_renderFinishedSemaphores;

    /// CPU-side fence: blocks the CPU until the GPU finishes frame slot [i].
    /// Starts in the signalled state so frame 0 doesn't block on the first wait.
    VkFence m_inFlightFences[MAX_FRAMES_IN_FLIGHT] = {};

    /// Which frame slot to use next (cycles 0 → 1 → 0 → …).
    uint32_t m_currentFrame = 0;

    /// Descriptor pool used exclusively by ImGui (font atlas sampler).
    VkDescriptorPool m_imguiDescriptorPool = VK_NULL_HANDLE;

    /// True only after both ImGui backends and the font texture are ready.
    bool m_imguiInitialized = false;

    /// Tracks whether the command buffer array was successfully allocated.
    bool m_commandBuffersAllocated = false;
};

#endif // FYP_VULKAN_RENDERER_RENDERER_H
