/**
 * @file Renderer.cpp
 * @brief Implementation of Renderer - per-frame command recording and GPU submission.
 *
 * This module drives the core Vulkan render loop for Milestone 1.  Every frame
 * follows the same ten-step sequence documented on drawFrame().  The key design
 * points are:
 *
 *   - Double-buffering (MAX_FRAMES_IN_FLIGHT = 2): while the GPU renders frame N,
 *     the CPU is recording frame N+1.  A per-slot fence throttles how far ahead
 *     the CPU can run.
 *
 *   - Dynamic Rendering (Vulkan 1.3): vkCmdBeginRendering / vkCmdEndRendering
 *     replace the old VkRenderPass / VkFramebuffer pattern.  The attachment is
 *     described inline per command buffer, not pre-baked.
 *
 *   - synchronization2: all image layout transitions use VkImageMemoryBarrier2
 *     and vkCmdPipelineBarrier2.  This gives finer-grained pipeline stage masks
 *     and clearer barrier semantics than the original barrier API.
 *
 * @author Mohamed Deeq Mohamed (P2884884)
 * @date   2026-04-10
 *
 * @see Renderer.h for the class interface.
 */

#include "Renderer.h"
#include "VulkanContext.h"
#include "SwapChain.h"
#include "Pipeline.h"

#include <spdlog/spdlog.h>

// ─── Lifecycle ────────────────────────────────────────────────────────────────

/// @details
/// Creates one command pool for the graphics queue family, then allocates
/// MAX_FRAMES_IN_FLIGHT command buffers from it.  Per-frame synchronisation
/// objects are created next:
///   - imageAvailableSemaphore: GPU signals this when a swapchain image is ready
///                              to be written to (after presentation is done).
///   - renderFinishedSemaphore: GPU signals this when the frame is fully rendered
///                              and the image is ready to be presented.
///   - inFlightFence:           starts in the SIGNALLED state so the first wait
///                              in drawFrame() returns immediately without blocking.
bool Renderer::init(const VulkanContext& ctx, const SwapChain& /*swap*/)
{
    // ── Command pool ──────────────────────────────────────────────────────
    // RESET_COMMAND_BUFFER_BIT lets us call vkResetCommandBuffer() per frame
    // instead of resetting the whole pool.
    VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = ctx.graphicsQueueFamily();

    if (vkCreateCommandPool(ctx.device(), &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS) {
        spdlog::error("Renderer: failed to create command pool");
        return false;
    }

    // ── Command buffers ───────────────────────────────────────────────────
    // One PRIMARY command buffer per in-flight frame slot.
    VkCommandBufferAllocateInfo allocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocInfo.commandPool        = m_commandPool;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

    if (vkAllocateCommandBuffers(ctx.device(), &allocInfo, m_commandBuffers) != VK_SUCCESS) {
        spdlog::error("Renderer: failed to allocate command buffers");
        return false;
    }

    // ── Per-frame sync objects ────────────────────────────────────────────
    VkSemaphoreCreateInfo semInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    // VK_FENCE_CREATE_SIGNALED_BIT: fence starts signalled so the first
    // vkWaitForFences() call in drawFrame() returns immediately.
    VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (vkCreateSemaphore(ctx.device(), &semInfo, nullptr,
                              &m_imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(ctx.device(), &semInfo, nullptr,
                              &m_renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(ctx.device(), &fenceInfo, nullptr,
                          &m_inFlightFences[i]) != VK_SUCCESS)
        {
            spdlog::error("Renderer: failed to create sync objects for frame {}", i);
            return false;
        }
    }

    spdlog::info("Renderer: initialised ({} frames in flight)", MAX_FRAMES_IN_FLIGHT);
    return true;
}

/// @details
/// Destroys sync objects first (semaphores + fences), then frees the command
/// buffers, then destroys the command pool.  Each handle is guarded with a
/// null-check so this is safe after a partial init() or multiple calls.
void Renderer::destroy(const VulkanContext& ctx)
{
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (m_renderFinishedSemaphores[i] != VK_NULL_HANDLE) {
            vkDestroySemaphore(ctx.device(), m_renderFinishedSemaphores[i], nullptr);
            m_renderFinishedSemaphores[i] = VK_NULL_HANDLE;
        }
        if (m_imageAvailableSemaphores[i] != VK_NULL_HANDLE) {
            vkDestroySemaphore(ctx.device(), m_imageAvailableSemaphores[i], nullptr);
            m_imageAvailableSemaphores[i] = VK_NULL_HANDLE;
        }
        if (m_inFlightFences[i] != VK_NULL_HANDLE) {
            vkDestroyFence(ctx.device(), m_inFlightFences[i], nullptr);
            m_inFlightFences[i] = VK_NULL_HANDLE;
        }
    }

    // Command buffers are freed implicitly when the pool is destroyed,
    // but explicit free keeps ownership clear.
    if (m_commandPool != VK_NULL_HANDLE) {
        vkFreeCommandBuffers(ctx.device(), m_commandPool,
                             MAX_FRAMES_IN_FLIGHT, m_commandBuffers);
        vkDestroyCommandPool(ctx.device(), m_commandPool, nullptr);
        m_commandPool = VK_NULL_HANDLE;
    }
}

// ─── Per-frame ────────────────────────────────────────────────────────────────

/// @details
/// Frame execution sequence:
///
///  Step 1  vkWaitForFences      — block CPU until the GPU has finished using
///                                 this frame slot's resources.
///  Step 2  acquireNextImage     — ask the swapchain for the next image to render
///                                 into.  Returns OUT_OF_DATE on window resize.
///  Step 3  vkResetFences        — reset the fence now we know we'll submit.
///  Step 4  Begin command buffer — ONE_TIME_SUBMIT: we reset+re-record every frame.
///  Step 5  Barrier: UNDEFINED → COLOR_ATTACHMENT_OPTIMAL
///                               — discard old contents, transition for writing.
///  Step 6  vkCmdBeginRendering  — Dynamic Rendering: describe the colour
///                                 attachment inline (CLEAR on load, STORE on end).
///  Step 7  Set viewport/scissor — dynamic state, matches current swapchain extent.
///  Step 8  Bind pipeline + draw — 3 vertices, no vertex buffer (hardcoded in shader).
///  Step 9  vkCmdEndRendering    — close the render pass.
///  Step 10 Barrier: COLOR_ATTACHMENT_OPTIMAL → PRESENT_SRC_KHR
///                               — transition the image for presentation.
///  Step 11 vkEndCommandBuffer
///  Step 12 vkQueueSubmit        — wait on imageAvailable, signal renderFinished.
///  Step 13 vkQueuePresentKHR    — wait on renderFinished, then flip.
///
/// Returns false if the swapchain is out of date (resize needed) or on error.
bool Renderer::drawFrame(const VulkanContext& ctx,
                          SwapChain&           swap,
                          const Pipeline&      pipeline)
{
    // ── Step 1: Wait for in-flight fence ──────────────────────────────────
    // This blocks the CPU until the GPU finishes all work submitted for this
    // frame slot.  On the very first frame the fence starts signalled, so
    // this returns immediately.
    vkWaitForFences(ctx.device(), 1, &m_inFlightFences[m_currentFrame],
                    VK_TRUE, UINT64_MAX);

    // ── Step 2: Acquire next swapchain image ──────────────────────────────
    // We signal imageAvailableSemaphores[currentFrame] when the driver has
    // finished presenting the previous content and the image is free to write.
    uint32_t imageIndex = 0;
    VkResult acquireResult = swap.acquireNextImage(
        ctx.device(),
        m_imageAvailableSemaphores[m_currentFrame],
        imageIndex);

    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
        // Swapchain is stale (window resized) — tell the caller to rebuild.
        // The fence was waited on above but NOT reset, so it remains signalled
        // and the next drawFrame() call on this slot will skip the wait.
        return false;
    }
    if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
        spdlog::error("Renderer: vkAcquireNextImageKHR error {}", static_cast<int>(acquireResult));
        return false;
    }

    // ── Step 3: Reset fence ───────────────────────────────────────────────
    // Only reset after a successful acquire — now we know we WILL submit
    // to the GPU and the fence will be signalled again at queue submit.
    vkResetFences(ctx.device(), 1, &m_inFlightFences[m_currentFrame]);

    // ── Step 4: Record command buffer ─────────────────────────────────────
    VkCommandBuffer cmd = m_commandBuffers[m_currentFrame];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    // ── Step 5: Barrier — UNDEFINED → COLOR_ATTACHMENT_OPTIMAL ───────────
    // We don't care about the old image contents (UNDEFINED layout), so we
    // discard them.  The barrier signals the COLOR_ATTACHMENT_OUTPUT stage
    // that the image is ready to be written to.
    //
    // synchronization2 barrier fields:
    //   srcStageMask  / srcAccessMask  — what MUST complete before the barrier
    //   dstStageMask  / dstAccessMask  — what MUST wait until after the barrier
    VkImage swapImage = swap.images()[imageIndex];

    VkImageMemoryBarrier2 toRender{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    toRender.srcStageMask  = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;  // nothing to wait for
    toRender.srcAccessMask = 0;
    toRender.dstStageMask  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    toRender.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    toRender.oldLayout     = VK_IMAGE_LAYOUT_UNDEFINED;             // discard old contents
    toRender.newLayout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    toRender.image         = swapImage;
    toRender.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    VkDependencyInfo dep1{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    dep1.imageMemoryBarrierCount = 1;
    dep1.pImageMemoryBarriers    = &toRender;
    vkCmdPipelineBarrier2(cmd, &dep1);

    // ── Step 6: Begin Dynamic Rendering ───────────────────────────────────
    // This replaces vkCmdBeginRenderPass.  The colour attachment is described
    // inline: CLEAR on load (black background), STORE on end (keep result).
    VkClearValue clearBlack{};
    // ✅ Explicit VkClearColorValue construction — MSVC + GCC compatible
    clearBlack.color = VkClearColorValue{{0.0f, 0.0f, 0.0f, 1.0f}};

    VkRenderingAttachmentInfo colourAttach{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    colourAttach.imageView   = swap.imageViews()[imageIndex];
    colourAttach.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colourAttach.loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR;   // clear to black
    colourAttach.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;  // keep rendered contents
    colourAttach.clearValue  = clearBlack;

    VkRenderingInfo renderingInfo{VK_STRUCTURE_TYPE_RENDERING_INFO};
    renderingInfo.renderArea           = {{0, 0}, swap.extent()};
    renderingInfo.layerCount           = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments    = &colourAttach;

    vkCmdBeginRendering(cmd, &renderingInfo);

    // ── Step 7: Dynamic viewport and scissor ──────────────────────────────
    // Match the current swapchain extent so we fill the whole window.
    // These must be set every frame because they are dynamic state.
    VkViewport viewport{};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = static_cast<float>(swap.extent().width);
    viewport.height   = static_cast<float>(swap.extent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{{0, 0}, swap.extent()};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // ── Step 8: Bind pipeline and draw ────────────────────────────────────
    // No vertex buffer — the 3 positions are a constant array in triangle.vert
    // indexed by gl_VertexIndex (0, 1, 2).  vkCmdDraw emits those 3 indices.
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.handle());
    vkCmdDraw(cmd, 3, 1, 0, 0); // vertexCount=3, instanceCount=1

    // ── Step 9: End rendering ─────────────────────────────────────────────
    vkCmdEndRendering(cmd);

    // ── Step 10: Barrier — COLOR_ATTACHMENT_OPTIMAL → PRESENT_SRC_KHR ────
    // Finished writing to the image; transition it for the presentation engine.
    // srcStageMask waits for colour attachment writes to finish before the
    // layout change, so the presentation engine sees the completed frame.
    VkImageMemoryBarrier2 toPresent{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    toPresent.srcStageMask  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    toPresent.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    toPresent.dstStageMask  = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT; // nothing reads after
    toPresent.dstAccessMask = 0;
    toPresent.oldLayout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    toPresent.newLayout     = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    toPresent.image         = swapImage;
    toPresent.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    VkDependencyInfo dep2{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    dep2.imageMemoryBarrierCount = 1;
    dep2.pImageMemoryBarriers    = &toPresent;
    vkCmdPipelineBarrier2(cmd, &dep2);

    // ── Step 11: End command buffer ───────────────────────────────────────
    vkEndCommandBuffer(cmd);

    // ── Step 12: Submit ───────────────────────────────────────────────────
    // WAIT on imageAvailable (image is free to write) before COLOR_ATTACHMENT_OUTPUT.
    // SIGNAL renderFinished when all commands complete.
    // SIGNAL inFlightFence so the CPU can reuse this frame slot next time.
    VkSemaphore          waitSems[]   = {m_imageAvailableSemaphores[m_currentFrame]};
    VkSemaphore          signalSems[] = {m_renderFinishedSemaphores[m_currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.waitSemaphoreCount   = 1;
    submitInfo.pWaitSemaphores      = waitSems;
    submitInfo.pWaitDstStageMask    = waitStages;
    submitInfo.commandBufferCount   = 1;
    submitInfo.pCommandBuffers      = &cmd;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores    = signalSems;

    if (vkQueueSubmit(ctx.graphicsQueue(), 1, &submitInfo,
                      m_inFlightFences[m_currentFrame]) != VK_SUCCESS) {
        spdlog::error("Renderer: vkQueueSubmit failed");
        return false;
    }

    // ── Step 13: Present ──────────────────────────────────────────────────
    // WAIT on renderFinished before the display engine reads the image.
    VkSwapchainKHR   swapchains[] = {swap.handle()};
    VkPresentInfoKHR presentInfo{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = signalSems;
    presentInfo.swapchainCount     = 1;
    presentInfo.pSwapchains        = swapchains;
    presentInfo.pImageIndices      = &imageIndex;

    VkResult presentResult = vkQueuePresentKHR(ctx.graphicsQueue(), &presentInfo);

    // Advance frame counter before returning so both success and rebuild
    // paths use different slots next time.
    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR ||
        presentResult == VK_SUBOPTIMAL_KHR) {
        return false; // caller rebuilds swapchain
    }
    if (presentResult != VK_SUCCESS) {
        spdlog::error("Renderer: vkQueuePresentKHR error {}", static_cast<int>(presentResult));
        return false;
    }

    return true;
}

/// @details
/// Blocks until all queued GPU work has completed.  Must be called before
/// destroying any resource that an in-flight command buffer might reference.
void Renderer::waitIdle(const VulkanContext& ctx)
{
    vkDeviceWaitIdle(ctx.device());
}
