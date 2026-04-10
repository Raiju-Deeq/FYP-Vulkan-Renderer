/**
 * @file SwapChain.h
 * @brief Vulkan swapchain creation, image-view management and resize handling.
 *
 * Wraps VkSwapchainKHR along with its VkImage / VkImageView arrays.  The
 * swapchain is tied to a surface and must be rebuilt whenever the window
 * is resized (call rebuild() on VK_ERROR_OUT_OF_DATE_KHR).
 *
 * @author Mohamed Deeq Mohamed
 * @date   2026-03-27
 */

#ifndef FYP_VULKAN_RENDERER_SWAPCHAIN_H
#define FYP_VULKAN_RENDERER_SWAPCHAIN_H

#include <vulkan/vulkan.h>
#include <vector>

class VulkanContext;

/**
 * @class SwapChain
 * @brief RAII wrapper for the Vulkan presentation swapchain.
 *
 * Owns the VkSwapchainKHR, the swapchain images and their image views.
 * Provides helpers to acquire the next image and query the current extent
 * so the rest of the pipeline does not need to cache those values.
 */
class SwapChain
{
public:
    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------

    /**
     * @brief Creates the swapchain, retrieves images and builds image views.
     *
     * Selects the best available surface format (preferring
     * VK_FORMAT_B8G8R8A8_SRGB + VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) and
     * present mode (preferring MAILBOX, falling back to FIFO).
     *
     * @param  ctx     Initialised VulkanContext providing device and surface.
     * @param  width   Initial framebuffer width in pixels.
     * @param  height  Initial framebuffer height in pixels.
     * @return true    on success.
     * @return false   if swapchain creation fails.
     */
    bool init(const VulkanContext& ctx, uint32_t width, uint32_t height);

    /**
     * @brief Destroys image views and the swapchain handle.
     * @param ctx  The same VulkanContext passed to init().
     */
    void destroy(const VulkanContext& ctx);

    /**
     * @brief Rebuilds the swapchain for a new window size (e.g. on resize).
     *
     * Internally calls destroy() then init() with the new dimensions.
     *
     * @param  ctx     Initialised VulkanContext.
     * @param  width   New framebuffer width in pixels.
     * @param  height  New framebuffer height in pixels.
     * @return true    on success.
     */
    bool rebuild(const VulkanContext& ctx, uint32_t width, uint32_t height);

    // -------------------------------------------------------------------------
    // Frame acquisition
    // -------------------------------------------------------------------------

    /**
     * @brief Acquires the next presentable image index from the swapchain.
     *
     * @param  device     Logical device handle.
     * @param  semaphore  Semaphore to signal when the image is available.
     * @param  outIndex   Receives the acquired image index.
     * @return VK_SUCCESS or VK_ERROR_OUT_OF_DATE_KHR (trigger rebuild).
     */
    VkResult acquireNextImage(VkDevice device, VkSemaphore semaphore,
                              uint32_t& outIndex);

    // -------------------------------------------------------------------------
    // Accessors
    // -------------------------------------------------------------------------

    /// @brief Returns the swapchain image format.
    VkFormat              format()     const { return m_format; }

    /// @brief Returns the current swapchain extent (width x height).
    VkExtent2D            extent()     const { return m_extent; }

    /// @brief Returns the raw swapchain handle.
    VkSwapchainKHR        handle()     const { return m_swapchain; }

    /// @brief Returns the array of swapchain image views.
    const std::vector<VkImageView>& imageViews() const { return m_imageViews; }

    /// @brief Returns the number of swapchain images.
    uint32_t              imageCount() const
        { return static_cast<uint32_t>(m_imageViews.size()); }

    /// @brief Returns the raw swapchain images (not owned — owned by the swapchain).
    /// @note  Required by Renderer to set up synchronization2 image layout barriers.
    const std::vector<VkImage>& images() const { return m_images; }

private:
    VkSwapchainKHR           m_swapchain  = VK_NULL_HANDLE;    ///< Raw swapchain handle.
    VkFormat                 m_format     = VK_FORMAT_UNDEFINED; ///< Selected image format.
    VkExtent2D               m_extent     = {0, 0};            ///< Current resolution.
    std::vector<VkImage>     m_images;                          ///< Swapchain images (not owned).
    std::vector<VkImageView> m_imageViews;                      ///< Image views (owned).
};

#endif // FYP_VULKAN_RENDERER_SWAPCHAIN_H
