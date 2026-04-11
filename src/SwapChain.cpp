/**
 * @file SwapChain.cpp
 * @brief Implementation of SwapChain — swapchain creation, image views and resize.
 *
 * ## Design note
 * This class deliberately leans on vk-bootstrap's `SwapchainBuilder` to
 * handle surface capability queries and format negotiation.  Without it you
 * would need to call:
 *  - `vkGetPhysicalDeviceSurfaceCapabilitiesKHR`
 *  - `vkGetPhysicalDeviceSurfaceFormatsKHR`
 *  - `vkGetPhysicalDeviceSurfacePresentModesKHR`
 *  - then rank and select from the results manually
 *
 * vk-bootstrap does all of that internally and exposes a clean builder API.
 *
 * @author Mohamed Deeq Mohamed (P2884884)
 * @date   2026-04-10
 * @see    SwapChain.h for the class interface and ownership model.
 * @see    https://github.com/charles-lunarg/vk-bootstrap (SwapchainBuilder docs)
 */

#include "SwapChain.h"
#include "VulkanContext.h"

#include <VkBootstrap.h>
#include <spdlog/spdlog.h>

// =============================================================================
// init()
// =============================================================================

/// @details
/// **Why `get_images()` and `get_image_views()` are separate calls:**
/// - `VkImage` handles are allocated by the driver when the swapchain is
///   created — we retrieve them with `vkGetSwapchainImagesKHR`.  We do NOT
///   own them; they are freed when the swapchain is destroyed.
/// - `VkImageView` handles must be created separately with `vkCreateImageView`
///   for each image.  We DO own these and must destroy them ourselves before
///   destroying the swapchain.
///
/// vk-bootstrap wraps both calls behind `get_images()` and `get_image_views()`
/// so we don't have to write the loops manually.
bool SwapChain::init(const VulkanContext& ctx, uint32_t width, uint32_t height)
{
    // Pass all three core context handles to the builder so it can query
    // GPU surface capabilities (the physical device) and create the
    // swapchain on the logical device using the target surface.
    vkb::SwapchainBuilder builder(ctx.physicalDevice(), ctx.device(), ctx.surface());

    auto swapResult = builder
        // Preferred format: B8G8R8A8_SRGB gives accurate sRGB output without
        // manual gamma correction in the shader.  SRGB_NONLINEAR_KHR is the
        // standard colour space for display output.
        .set_desired_format({VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
        // MAILBOX: driver renders frames as fast as possible, discarding
        // older pending frames when a new one is ready — low latency, no tearing.
        // FIFO fallback: classic vsync — always supported, slightly higher latency.
        .set_desired_present_mode(VK_PRESENT_MODE_MAILBOX_KHR)
        // The driver clamps the extent to the surface min/max capabilities,
        // so passing the current framebuffer size is always safe.
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

    // Retrieve the VkImages the driver created for us.
    // These are NOT owned by this class — the driver reclaims them when the
    // swapchain is destroyed.  We store them so Renderer can record image
    // layout transition barriers (it needs the raw VkImage handle).
    auto imagesResult = vkbSwap.get_images();
    if (!imagesResult) {
        spdlog::error("SwapChain: failed to retrieve images: {}", imagesResult.error().message());
        return false;
    }
    m_images = imagesResult.value();

    // Retrieve the VkImageViews that vk-bootstrap created alongside the images.
    // Image views are thin wrappers that tell the driver *how* to interpret
    // the raw image data — format, aspect (colour vs depth), mip/array ranges.
    // Rendering commands reference image views, not raw images.
    // We OWN these and must destroy them before destroying the swapchain.
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

// =============================================================================
// destroy()
// =============================================================================

/// @details
/// **Teardown order matters:**
/// Image views must be destroyed before the swapchain because they reference
/// the swapchain's VkImage handles.  If the swapchain were destroyed first,
/// those VkImage handles would become invalid while the views still point at
/// them — a use-after-free that the validation layer would catch.
void SwapChain::destroy(const VulkanContext& ctx)
{
    // Destroy each image view.  The raw VkImages are owned by the swapchain
    // (not by us) so we intentionally do NOT call vkDestroyImage here.
    for (VkImageView view : m_imageViews) {
        vkDestroyImageView(ctx.device(), view, nullptr);
    }
    m_imageViews.clear();
    m_images.clear(); // just clearing the handles — we never owned these

    if (m_swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(ctx.device(), m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;
    }

    // Reset metadata to safe defaults so any accidental access is obvious.
    m_format = VK_FORMAT_UNDEFINED;
    m_extent = {0, 0};
}

// =============================================================================
// rebuild()
// =============================================================================

/// @details
/// Called whenever `drawFrame()` receives `VK_ERROR_OUT_OF_DATE_KHR` from
/// either `vkAcquireNextImageKHR` or `vkQueuePresentKHR`.  Both signals mean
/// the existing swapchain dimensions no longer match the window.
///
/// Renderer::waitIdle() must be called by the caller *before* rebuild() to
/// ensure the GPU has finished reading from any in-flight images.  Destroying
/// the swapchain while the GPU is still referencing its images is undefined
/// behaviour.
bool SwapChain::rebuild(const VulkanContext& ctx, uint32_t width, uint32_t height)
{
    spdlog::info("SwapChain: rebuilding at {}x{}", width, height);
    destroy(ctx);
    return init(ctx, width, height);
}

// =============================================================================
// acquireNextImage()
// =============================================================================

/// @details
/// **What this call actually does on the GPU:**
/// `vkAcquireNextImageKHR` queues a GPU-side signal operation on `semaphore`.
/// From the CPU's perspective the call returns immediately with an image index.
/// The semaphore is only signalled when the presentation engine has actually
/// finished reading the image from the previous frame and the GPU can safely
/// write into it.
///
/// You *must* wait on that semaphore in your submit's `pWaitSemaphores` before
/// recording any rendering commands that write to the image.  If you draw
/// without waiting you risk writing into an image the display is still showing.
///
/// `UINT64_MAX` is the standard timeout value meaning "wait forever".  In
/// practice, if the swapchain is healthy this returns near-instantly.
VkResult SwapChain::acquireNextImage(VkDevice device,
                                     VkSemaphore semaphore,
                                     uint32_t& outIndex)
{
    return vkAcquireNextImageKHR(device, m_swapchain, UINT64_MAX,
                                  semaphore, VK_NULL_HANDLE, &outIndex);
}
