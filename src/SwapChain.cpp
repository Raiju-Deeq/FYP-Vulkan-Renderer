/**
 * @file SwapChain.cpp
 * @brief Implementation of SwapChain - swapchain creation and image-view management.
 *
 * Uses vk-bootstrap's SwapchainBuilder to handle the surface-format negotiation,
 * present-mode selection and image retrieval.  The class owns the VkImageViews
 * but NOT the VkImages themselves — those are owned by the swapchain driver.
 *
 * Teardown (destroy) must be called before VulkanContext::destroy() because
 * the swapchain holds a reference to the logical device.
 *
 * @author Mohamed Deeq Mohamed (P2884884)
 * @date   2026-04-10
 *
 * @see SwapChain.h for the class interface.
 * @see https://github.com/charles-lunarg/vk-bootstrap for SwapchainBuilder API.
 */

#include "SwapChain.h"
#include "VulkanContext.h"

#include <VkBootstrap.h>
#include <spdlog/spdlog.h>

/// @details
/// Uses vkb::SwapchainBuilder to negotiate:
///   - Surface format: VK_FORMAT_B8G8R8A8_SRGB + SRGB_NONLINEAR (common, widely supported)
///   - Present mode:   MAILBOX (low-latency triple-buffering) → FIFO fallback (vsync, always available)
///   - Extent:         clamped to the surface capabilities by vk-bootstrap
///
/// After building, the VkImages are retrieved from the driver (not owned by us)
/// and one VkImageView is created per image.  Image views are what the rendering
/// commands actually reference — they describe how to interpret the raw image data.
bool SwapChain::init(const VulkanContext& ctx, uint32_t width, uint32_t height)
{
    vkb::SwapchainBuilder builder(ctx.physicalDevice(), ctx.device(), ctx.surface());

    auto swapResult = builder
        .set_desired_format({VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
        .set_desired_present_mode(VK_PRESENT_MODE_MAILBOX_KHR)   // low-latency; falls back to FIFO
        .set_desired_extent(width, height)
        .build();

    if (!swapResult) {
        spdlog::error("SwapChain: failed to create swapchain: {}", swapResult.error().message());
        return false;
    }

    vkb::Swapchain vkbSwap = swapResult.value();
    m_swapchain = vkbSwap.swapchain;
    m_format    = vkbSwap.image_format;
    m_extent    = vkbSwap.extent;

    // Retrieve the VkImages the driver created for us.  These are NOT owned by
    // this class — destroying the swapchain reclaims them automatically.
    auto imagesResult = vkbSwap.get_images();
    if (!imagesResult) {
        spdlog::error("SwapChain: failed to retrieve images: {}", imagesResult.error().message());
        return false;
    }
    m_images = imagesResult.value();

    // Retrieve the VkImageViews vk-bootstrap created for the images above.
    // These ARE owned by us and must be destroyed before the swapchain.
    auto viewsResult = vkbSwap.get_image_views();
    if (!viewsResult) {
        spdlog::error("SwapChain: failed to retrieve image views: {}", viewsResult.error().message());
        return false;
    }
    m_imageViews = viewsResult.value();

    spdlog::info("SwapChain: {}x{}  images={}  format={}",
        m_extent.width, m_extent.height,
        m_images.size(),
        static_cast<int>(m_format));

    return true;
}

/// @details
/// Destroys image views first (they reference the images), then the swapchain
/// (which implicitly reclaims the VkImages).  Each handle is guarded with a
/// null-check so this is safe to call after a partial init() or multiple times.
void SwapChain::destroy(const VulkanContext& ctx)
{
    for (VkImageView view : m_imageViews) {
        vkDestroyImageView(ctx.device(), view, nullptr);
    }
    m_imageViews.clear();
    m_images.clear();

    if (m_swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(ctx.device(), m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;
    }

    m_format = VK_FORMAT_UNDEFINED;
    m_extent = {0, 0};
}

/// @details
/// Calls destroy() then init() with the new dimensions.  Used after the window
/// is resized (VK_ERROR_OUT_OF_DATE_KHR from acquire or present).
bool SwapChain::rebuild(const VulkanContext& ctx, uint32_t width, uint32_t height)
{
    spdlog::info("SwapChain: rebuilding at {}x{}", width, height);
    destroy(ctx);
    return init(ctx, width, height);
}

/// @details
/// Wraps vkAcquireNextImageKHR with an infinite timeout.  The semaphore is
/// signalled by the driver when the image is no longer being read for presentation
/// and is safe to write to.  Returns VK_ERROR_OUT_OF_DATE_KHR on window resize.
VkResult SwapChain::acquireNextImage(VkDevice device,
                                     VkSemaphore semaphore,
                                     uint32_t& outIndex)
{
    return vkAcquireNextImageKHR(device, m_swapchain, UINT64_MAX,
                                  semaphore, VK_NULL_HANDLE, &outIndex);
}
