/**
 * @file SwapChain.cpp
 * @brief Implementation of SwapChain — swapchain and image-view management.
 *
 * @author Mohamed Deeq Mohamed
 * @date   2026-03-27
 *
 * @todo  Week 2 — implement init() using vkCreateSwapchainKHR and
 *                 vkGetSwapchainImagesKHR, then build one VkImageView per image.
 * @todo  Week 2 — implement destroy() and rebuild() wrappers.
 */

#include "SwapChain.h"
#include "VulkanContext.h"

bool SwapChain::init(const VulkanContext& /*ctx*/,
                     uint32_t /*width*/, uint32_t /*height*/)
{
    // TODO: Week 2 — vkCreateSwapchainKHR + image view creation
    return false;
}

void SwapChain::destroy(const VulkanContext& /*ctx*/)
{
    // TODO: Week 2 — destroy image views then swapchain
}

bool SwapChain::rebuild(const VulkanContext& ctx,
                        uint32_t width, uint32_t height)
{
    destroy(ctx);
    return init(ctx, width, height);
}

VkResult SwapChain::acquireNextImage(VkDevice /*device*/,
                                     VkSemaphore /*semaphore*/,
                                     uint32_t& /*outIndex*/)
{
    // TODO: Week 2 — vkAcquireNextImageKHR
    return VK_ERROR_INITIALIZATION_FAILED;
}
