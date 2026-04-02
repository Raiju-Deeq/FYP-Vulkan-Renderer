/**
 * @file Renderer.h
 * @brief Top-level renderer - per-frame record, submit and present loop.
 *
 * Renderer is the main entry point for the render loop.  It owns the
 * per-frame synchronisation objects (semaphores, fences), the command
 * pool and command buffers, and orchestrates calls to SwapChain and
 * Pipeline on each frame.
 *
 * @author Mohamed Deeq Mohamed
 * @date   2026-03-27
 */

#ifndef FYP_VULKAN_RENDERER_RENDERER_H
#define FYP_VULKAN_RENDERER_RENDERER_H

#include <vulkan/vulkan.h>
#include <cstdint>

class VulkanContext;
class SwapChain;
class Pipeline;

/// @brief Maximum number of frames processed concurrently (double-buffered).
static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

/**
 * @class Renderer
 * @brief Owns per-frame GPU resources and drives the main render loop.
 *
 * One Renderer instance is created after VulkanContext and SwapChain are
 * initialised.  Call drawFrame() once per application tick inside the
 * main loop.  Call waitIdle() before destroying any resources.
 */
class Renderer
{
public:
    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------

    /**
     * @brief Allocates command pool, command buffers and sync objects.
     *
     * @param  ctx   Initialised VulkanContext.
     * @param  swap  Initialised SwapChain (frame count is read from it).
     * @return true  on success.
     * @return false if any Vulkan allocation fails.
     */
    bool init(const VulkanContext& ctx, const SwapChain& swap);

    /**
     * @brief Destroys semaphores, fences, command buffers and the command pool.
     * @param ctx  The same VulkanContext passed to init().
     */
    void destroy(const VulkanContext& ctx);

    // -------------------------------------------------------------------------
    // Per-frame
    // -------------------------------------------------------------------------

    /**
     * @brief Records and submits a single frame, then queues presentation.
     *
     * Frame flow:
     *  1. Wait for in-flight fence.
     *  2. Acquire next swapchain image.
     *  3. Begin command buffer recording.
     *  4. Transition image layout (UNDEFINED -> COLOR_ATTACHMENT_OPTIMAL).
     *  5. Begin Dynamic Rendering pass (vkCmdBeginRenderingKHR).
     *  6. Bind pipeline and draw.
     *  7. End Dynamic Rendering pass.
     *  8. Transition image layout (COLOR_ATTACHMENT_OPTIMAL -> PRESENT_SRC_KHR).
     *  9. Submit command buffer.
     * 10. Present.
     *
     * @param  ctx      Initialised VulkanContext.
     * @param  swap     Initialised SwapChain.
     * @param  pipeline Compiled Pipeline.
     * @return true     on success.
     * @return false    on VK_ERROR_OUT_OF_DATE_KHR or other fatal errors.
     */
    bool drawFrame(const VulkanContext& ctx,
                   SwapChain&           swap,
                   const Pipeline&      pipeline);

    /**
     * @brief Blocks until the GPU has finished all in-flight work.
     *
     * Must be called before destroying the SwapChain, Pipeline or any
     * resource that a command buffer might still reference.
     *
     * @param ctx  Initialised VulkanContext.
     */
    void waitIdle(const VulkanContext& ctx);

private:
    VkCommandPool   m_commandPool = VK_NULL_HANDLE; ///< Pool for graphics command buffers.

    /// Per-frame command buffers (size = MAX_FRAMES_IN_FLIGHT).
    VkCommandBuffer m_commandBuffers[MAX_FRAMES_IN_FLIGHT] = {};

    /// Semaphore signalled when a swapchain image is available per frame.
    VkSemaphore     m_imageAvailableSemaphores[MAX_FRAMES_IN_FLIGHT] = {};

    /// Semaphore signalled when rendering is complete and image can be presented.
    VkSemaphore     m_renderFinishedSemaphores[MAX_FRAMES_IN_FLIGHT] = {};

    /// Fence used to throttle CPU ahead of GPU (one per in-flight frame).
    VkFence         m_inFlightFences[MAX_FRAMES_IN_FLIGHT] = {};

    uint32_t        m_currentFrame = 0; ///< Frame index (0 ... MAX_FRAMES_IN_FLIGHT-1).
};

#endif // FYP_VULKAN_RENDERER_RENDERER_H
