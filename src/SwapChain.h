/**
 * @file SwapChain.h
 * @brief Vulkan swapchain: the queue of images that get presented to my screen.
 *
 * ## What a swapchain is
 * A swapchain is a ring of images that sit between the GPU and the display.
 * At any point in time one image is being *displayed*, one or more are
 * *in-flight* (the GPU is drawing into them), and the rest are *available*
 * (waiting for me to draw into).
 *
 * The basic cycle I follow each frame is:
 *  1. **Acquire** — ask the swapchain for the next available image index.
 *  2. **Draw** — record GPU commands that write into that image.
 *  3. **Present** — hand the finished image back to the display engine.
 *
 * ## Why 4 images?
 * I request 3 images (triple-buffering) but the driver may grant more if
 * MAILBOX present mode is selected (it needs a spare image to replace the
 * pending frame without tearing). On my RX 5700 XT + Wayland setup, the
 * driver allocates 4.
 *
 * ## Resize handling
 * When my OS window is resized the swapchain becomes *out of date* — the
 * image dimensions no longer match the window. The driver signals this by
 * returning `VK_ERROR_OUT_OF_DATE_KHR` from either acquire or present. I
 * then call rebuild() with the new dimensions.
 *
 * @author Mohamed Deeq Mohamed (P2884884)
 * @date   2026-03-27
 */

#ifndef FYP_VULKAN_RENDERER_SWAPCHAIN_H
#define FYP_VULKAN_RENDERER_SWAPCHAIN_H

#include <vulkan/vulkan.h>
#include <vector>

class VulkanContext;

/**
 * @class SwapChain
 * @brief RAII wrapper for `VkSwapchainKHR`, its images and image views.
 *
 * ## Ownership model
 * - **I own:** `VkSwapchainKHR`, `VkImageView[]`.
 * - **I do NOT own:** `VkImage[]`. Swapchain images belong to the driver;
 *   they are destroyed automatically when I destroy the swapchain.
 *
 * ## Dependency
 * I require VulkanContext to be fully initialised first (I need the device,
 * physical device and surface handles).
 *
 * ## Usage
 * @code
 *   SwapChain swap;
 *   swap.init(ctx, 1280, 720);
 *
 *   // Per-frame:
 *   uint32_t imageIndex;
 *   VkResult r = swap.acquireNextImage(ctx.device(), semaphore, imageIndex);
 *   if (r == VK_ERROR_OUT_OF_DATE_KHR) { swap.rebuild(ctx, w, h); }
 *
 *   // Teardown:
 *   swap.destroy(ctx);
 * @endcode
 */
class SwapChain
{
public:
    // =========================================================================
    // Lifecycle
    // =========================================================================

    /**
     * @brief Creates the swapchain, retrieves images, and builds image views.
     *
     * I use vk-bootstrap's `SwapchainBuilder` to handle surface-format
     * negotiation and present-mode selection automatically.
     *
     * **Format preference:**
     * I request `VK_FORMAT_B8G8R8A8_SRGB` + `VK_COLOR_SPACE_SRGB_NONLINEAR_KHR`.
     * This is the most widely supported sRGB swapchain format. vk-bootstrap
     * falls back to whatever the driver offers if this exact format isn't
     * available.
     *
     * **Present mode preference:**
     * - `MAILBOX` (triple-buffering, low latency, no tearing) if available.
     * - `FIFO` (vsync, always available) as fallback.
     *
     * **Image views:**
     * Raw `VkImage`s can't be used directly in my rendering commands — I need
     * an image view that describes *how* to interpret the image data (format,
     * aspect flags, mip/array ranges). I create one image view per image.
     *
     * @param  ctx     Fully initialised VulkanContext.
     * @param  width   Initial framebuffer width in pixels.
     * @param  height  Initial framebuffer height in pixels.
     * @return true    on success.
     * @return false   if swapchain creation or image view retrieval fails.
     */
    bool init(const VulkanContext& ctx, uint32_t width, uint32_t height);

    /**
     * @brief Destroys all owned Vulkan objects.
     *
     * I destroy image views first (they reference the images), then the
     * swapchain itself (which implicitly reclaims the `VkImage` objects).
     *
     * @param ctx  The same VulkanContext that was passed to init().
     */
    void destroy(const VulkanContext& ctx);

    /**
     * @brief Tears down and recreates the swapchain at new dimensions.
     *
     * I call this when the window is resized and `VK_ERROR_OUT_OF_DATE_KHR`
     * comes back from acquire or present. Internally calls destroy() then
     * init(), which rebuilds all image views at the new size.
     *
     * @param  ctx     Initialised VulkanContext.
     * @param  width   New framebuffer width in pixels.
     * @param  height  New framebuffer height in pixels.
     * @return true    on success.
     */
    bool rebuild(const VulkanContext& ctx, uint32_t width, uint32_t height);

    // =========================================================================
    // Per-frame acquisition
    // =========================================================================

    /**
     * @brief Asks the driver for the next image I can draw into.
     *
     * This is a non-blocking call from the CPU's perspective (it queues a
     * GPU-side wait). The driver returns an image index immediately, but
     * the image may not yet be free. The `semaphore` is signalled by the
     * driver when the image is *actually* safe to write into — I must wait
     * on that semaphore in my queue submit before drawing.
     *
     * @param  device    Logical device handle.
     * @param  semaphore Semaphore the driver will signal when the image is free.
     *                   I pass `m_imageAvailableSemaphores[currentFrame]` from Renderer.
     * @param  outIndex  Receives the acquired image index (I use this to index
     *                   into imageViews() when recording draw commands).
     * @return `VK_SUCCESS` or `VK_SUBOPTIMAL_KHR` on normal acquisition.
     * @return `VK_ERROR_OUT_OF_DATE_KHR` if my window was resized — I must
     *         rebuild the swapchain and skip the current frame.
     */
    VkResult acquireNextImage(VkDevice device, VkSemaphore semaphore,
                              uint32_t& outIndex);

    // =========================================================================
    // Accessors
    // =========================================================================

    /// @brief Pixel format of my swapchain images (e.g. `VK_FORMAT_B8G8R8A8_SRGB`).
    /// I pass this to Pipeline::init() so the graphics pipeline targets the correct format.
    VkFormat              format()     const { return m_format; }

    /// @brief Current swapchain resolution in pixels.
    /// I use this in Renderer to set the viewport and scissor rect each frame.
    VkExtent2D            extent()     const { return m_extent; }

    /// @brief Raw swapchain handle.
    /// I pass this to vkQueuePresentKHR in Renderer.
    VkSwapchainKHR        handle()     const { return m_swapchain; }

    /// @brief Image views — one per swapchain image.
    /// I index this with the acquired imageIndex to form the colour attachment
    /// descriptor I pass to vkCmdBeginRendering.
    const std::vector<VkImageView>& imageViews() const { return m_imageViews; }

    /// @brief Number of swapchain images.
    /// I use this in Renderer to allocate one renderFinished semaphore per image.
    uint32_t              imageCount() const
        { return static_cast<uint32_t>(m_images.size()); }

    /// @brief Raw swapchain image handles (not owned — owned by the driver).
    /// I need these in Renderer to record image layout transition barriers
    /// (UNDEFINED → COLOR_ATTACHMENT_OPTIMAL and back to PRESENT_SRC_KHR).
    const std::vector<VkImage>& images() const { return m_images; }

private:
    VkSwapchainKHR           m_swapchain  = VK_NULL_HANDLE;      ///< My owned swapchain handle.
    VkFormat                 m_format     = VK_FORMAT_UNDEFINED;  ///< Negotiated image format.
    VkExtent2D               m_extent     = {0, 0};               ///< Current pixel dimensions.
    std::vector<VkImage>     m_images;   ///< Driver-owned swapchain images (I must NOT destroy these).
    std::vector<VkImageView> m_imageViews; ///< My owned views — one per image, same lifetime.
};

#endif // FYP_VULKAN_RENDERER_SWAPCHAIN_H
