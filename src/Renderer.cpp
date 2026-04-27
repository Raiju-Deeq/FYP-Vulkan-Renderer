/**
 * @file Renderer.cpp
 * @brief Implementation of Renderer — per-frame command recording and GPU submission.
 *
 * ## Reading guide
 * The heart of this file is `drawFrame()`.  Read it top-to-bottom alongside
 * the step numbers in the header doc and you will see the complete Vulkan
 * frame lifecycle.  Every step has a comment explaining *why* it exists and
 * what would go wrong if you removed it.
 *
 * ## Double-buffering mental model
 * Imagine two "slots" (frame 0 and frame 1).  Each slot has its own command
 * buffer, semaphores and fence.  While the GPU executes slot 0, the CPU is
 * recording into slot 1's command buffer.  The fence for slot 0 prevents the
 * CPU from reusing slot 0's resources until the GPU finishes.
 *
 * ```
 * Frame:    0     1     2     3     4 ...
 * CPU slot: 0     1     0     1     0 ...  (m_currentFrame cycles 0→1→0→...)
 * GPU:         [0]   [1]   [2]   [3]   ...
 * ```
 *
 * ## Image layout transitions
 * Vulkan images have a *layout* — an internal arrangement of pixels in memory
 * that is optimised for a specific purpose:
 *
 * | Layout                            | Optimal for           |
 * |-----------------------------------|-----------------------|
 * | `VK_IMAGE_LAYOUT_UNDEFINED`       | "Don't care"          |
 * | `VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL` | Writing from shaders |
 * | `VK_IMAGE_LAYOUT_PRESENT_SRC_KHR` | Presentation engine   |
 *
 * Transitioning between layouts requires a `VkImageMemoryBarrier2` — it both
 * changes the layout and inserts a memory barrier so previous writes are
 * visible to subsequent reads.
 *
 * @author Mohamed Deeq Mohamed (P2884884)
 * @date   2026-04-10
 * @see    Renderer.h for the class interface and full step-by-step drawFrame docs.
 */

#include "Renderer.h"
#include "VulkanContext.h"
#include "SwapChain.h"
#include "Pipeline.h"
#include "Mesh.h"
#include "Material.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <spdlog/spdlog.h>

// =============================================================================
// init()
// =============================================================================

/// @details
/// **Why RESET_COMMAND_BUFFER_BIT on the pool?**
/// Without this flag, command buffers from the pool can only be reset by
/// resetting the *entire* pool — all buffers at once.  The flag lets me call
/// `vkResetCommandBuffer` per-buffer each frame, which is more flexible.
///
/// **Why do fences start signalled?**
/// `drawFrame()` calls `vkWaitForFences` at the *start* of each frame.  On
/// the very first frame, no GPU work has been submitted yet.  If fence[0]
/// started unsignalled, the CPU would block forever waiting for GPU work that
/// was never submitted.  Starting signalled means the first wait returns
/// immediately, and the fence is reset only after a successful acquire.
bool Renderer::init(const VulkanContext& ctx, const SwapChain& swap)
{
    // ── Command pool ──────────────────────────────────────────────────────
    // The pool is tied to one queue family.  All command buffers allocated
    // from it must be submitted to queues in that same family.
    VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = ctx.graphicsQueueFamily();

    if (vkCreateCommandPool(ctx.device(), &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS) {
        spdlog::error("Renderer: failed to create command pool");
        return false;
    }

    // ── Command buffers ───────────────────────────────────────────────────
    // Allocate MAX_FRAMES_IN_FLIGHT primary command buffers in one call.
    // PRIMARY means these buffers can be submitted directly to a queue.
    // (SECONDARY buffers can only be called from primary ones — not used here.)
    VkCommandBufferAllocateInfo allocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocInfo.commandPool        = m_commandPool;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

    if (vkAllocateCommandBuffers(ctx.device(), &allocInfo, m_commandBuffers) != VK_SUCCESS) {
        spdlog::error("Renderer: failed to allocate command buffers");
        destroy(ctx);
        return false;
    }
    m_commandBuffersAllocated = true;

    // ── Per-frame sync objects ────────────────────────────────────────────
    VkSemaphoreCreateInfo semInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};

    // SIGNALED_BIT: fence starts signalled so the first vkWaitForFences in
    // drawFrame() returns immediately (see reasoning above).
    VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (vkCreateSemaphore(ctx.device(), &semInfo, nullptr,
                              &m_imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(ctx.device(), &fenceInfo, nullptr,
                          &m_inFlightFences[i]) != VK_SUCCESS)
        {
            spdlog::error("Renderer: failed to create sync objects for frame {}", i);
            destroy(ctx);
            return false;
        }
    }

    if (!recreateSwapchainSync(ctx, swap)) {
        destroy(ctx);
        return false;
    }

    spdlog::info("Renderer: initialised ({} frames in flight, {} swapchain images)",
                 MAX_FRAMES_IN_FLIGHT, swap.imageCount());
    return true;
}

// =============================================================================
// destroy()
// =============================================================================

/// @details
/// I destroy semaphores and fences before the command pool.  Destroying
/// the pool also implicitly frees all command buffers allocated from it, but
/// I call `vkFreeCommandBuffers` explicitly first to keep the ownership chain
/// clear and avoid confusion if this is extended later.
///
/// Always call `waitIdle()` before `destroy()` — if the GPU is mid-frame when
/// I destroy a semaphore, the behaviour is undefined.
void Renderer::destroy(const VulkanContext& ctx)
{
    // renderFinished semaphores — one per swapchain image
    for (VkSemaphore& sem : m_renderFinishedSemaphores) {
        if (sem != VK_NULL_HANDLE) {
            vkDestroySemaphore(ctx.device(), sem, nullptr);
            sem = VK_NULL_HANDLE;
        }
    }
    m_renderFinishedSemaphores.clear();

    // Per-frame semaphores and fences
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (m_imageAvailableSemaphores[i] != VK_NULL_HANDLE) {
            vkDestroySemaphore(ctx.device(), m_imageAvailableSemaphores[i], nullptr);
            m_imageAvailableSemaphores[i] = VK_NULL_HANDLE;
        }
        if (m_inFlightFences[i] != VK_NULL_HANDLE) {
            vkDestroyFence(ctx.device(), m_inFlightFences[i], nullptr);
            m_inFlightFences[i] = VK_NULL_HANDLE;
        }
    }

    // Command buffers are freed implicitly when the pool is destroyed, but
    // I free them explicitly first to keep ownership intent readable.
    if (m_commandPool != VK_NULL_HANDLE && m_commandBuffersAllocated) {
        vkFreeCommandBuffers(ctx.device(), m_commandPool,
                             MAX_FRAMES_IN_FLIGHT, m_commandBuffers);
        for (VkCommandBuffer& cmd : m_commandBuffers) {
            cmd = VK_NULL_HANDLE;
        }
        m_commandBuffersAllocated = false;
    }

    if (m_commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(ctx.device(), m_commandPool, nullptr);
        m_commandPool = VK_NULL_HANDLE;
    }
}

// =============================================================================
// recreateSwapchainSync()
// =============================================================================

/// @details
/// `renderFinished` is the only sync array sized from the swapchain.  A resize
/// can legally change the number of images the driver gives me, so keeping the
/// old vector would make `drawFrame()` index the wrong semaphore array.
///
/// The caller must wait for the device to be idle first.  That guarantees no
/// presentation operation still owns one of the old semaphores.
bool Renderer::recreateSwapchainSync(const VulkanContext& ctx, const SwapChain& swap)
{
    for (VkSemaphore& sem : m_renderFinishedSemaphores) {
        if (sem != VK_NULL_HANDLE) {
            vkDestroySemaphore(ctx.device(), sem, nullptr);
            sem = VK_NULL_HANDLE;
        }
    }

    const uint32_t imageCount = swap.imageCount();
    m_renderFinishedSemaphores.assign(imageCount, VK_NULL_HANDLE);

    VkSemaphoreCreateInfo semInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    for (uint32_t i = 0; i < imageCount; ++i) {
        if (vkCreateSemaphore(ctx.device(), &semInfo, nullptr,
                              &m_renderFinishedSemaphores[i]) != VK_SUCCESS)
        {
            spdlog::error("Renderer: failed to create renderFinished semaphore {}", i);
            return false;
        }
    }

    spdlog::info("Renderer: rebuilt per-image sync for {} swapchain images", imageCount);
    return true;
}

// =============================================================================
// initImGui()
// =============================================================================

bool Renderer::initImGui(const VulkanContext& ctx, const SwapChain& swap, GLFWwindow* window)
{
    // ImGui has its own descriptor pool because it manages descriptor sets for
    // the font atlas internally.  Keeping that pool separate stops the debug UI
    // from consuming descriptor capacity meant for my renderer's own materials.
    //
    // For now the overlay only uses the font texture, so one combined image
    // sampler descriptor is enough.
    VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1};
    VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfo.flags        = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets      = 1;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes   = &poolSize;

    if (vkCreateDescriptorPool(ctx.device(), &poolInfo, nullptr, &m_imguiDescriptorPool) != VK_SUCCESS) {
        spdlog::error("Renderer: failed to create ImGui descriptor pool");
        return false;
    }

    ImGui::CreateContext();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    if (!ImGui_ImplGlfw_InitForVulkan(window, true)) {
        spdlog::error("Renderer: ImGui_ImplGlfw_InitForVulkan failed");
        ImGui::DestroyContext();
        vkDestroyDescriptorPool(ctx.device(), m_imguiDescriptorPool, nullptr);
        m_imguiDescriptorPool = VK_NULL_HANDLE;
        return false;
    }

    // Dynamic rendering: no VkRenderPass.  The backend still needs to know the
    // swapchain colour format so it can build its own small UI pipeline.
    //
    // If the swapchain format ever changes on rebuild, the ImGui backend should
    // be recreated too.  Most drivers keep the format stable, but this is worth
    // keeping in mind when tightening resize handling for the final report.
    VkFormat colorFormat = swap.format();
    VkPipelineRenderingCreateInfoKHR pipelineRenderingInfo{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR};
    pipelineRenderingInfo.colorAttachmentCount    = 1;
    pipelineRenderingInfo.pColorAttachmentFormats = &colorFormat;

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance                    = ctx.instance();
    initInfo.PhysicalDevice              = ctx.physicalDevice();
    initInfo.Device                      = ctx.device();
    initInfo.QueueFamily                 = ctx.graphicsQueueFamily();
    initInfo.Queue                       = ctx.graphicsQueue();
    initInfo.DescriptorPool              = m_imguiDescriptorPool;
    initInfo.MinImageCount               = 2;
    initInfo.ImageCount                  = swap.imageCount();
    initInfo.MSAASamples                 = VK_SAMPLE_COUNT_1_BIT;
    initInfo.UseDynamicRendering         = true;
    initInfo.PipelineRenderingCreateInfo = pipelineRenderingInfo;

    if (!ImGui_ImplVulkan_Init(&initInfo)) {
        spdlog::error("Renderer: ImGui_ImplVulkan_Init failed");
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        vkDestroyDescriptorPool(ctx.device(), m_imguiDescriptorPool, nullptr);
        m_imguiDescriptorPool = VK_NULL_HANDLE;
        return false;
    }

    if (!ImGui_ImplVulkan_CreateFontsTexture()) {
        spdlog::error("Renderer: ImGui_ImplVulkan_CreateFontsTexture failed");
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        vkDestroyDescriptorPool(ctx.device(), m_imguiDescriptorPool, nullptr);
        m_imguiDescriptorPool = VK_NULL_HANDLE;
        return false;
    }

    m_imguiInitialized = true;

    spdlog::info("Renderer: ImGui initialised (dynamic rendering, imgui {})", ImGui::GetVersion());
    return true;
}

// =============================================================================
// recreateImGui()
// =============================================================================

bool Renderer::recreateImGui(const VulkanContext& ctx, const SwapChain& swap, GLFWwindow* window)
{
    shutdownImGui(ctx);
    return initImGui(ctx, swap, window);
}

// =============================================================================
// shutdownImGui()
// =============================================================================

void Renderer::shutdownImGui(const VulkanContext& ctx)
{
    if (m_imguiInitialized) {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        m_imguiInitialized = false;
    }

    if (m_imguiDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(ctx.device(), m_imguiDescriptorPool, nullptr);
        m_imguiDescriptorPool = VK_NULL_HANDLE;
    }
}

// =============================================================================
// drawFrame()
// =============================================================================

/// @details
/// See Renderer.h for the full 13-step sequence.  The inline comments below
/// focus on the *why* of each step.
bool Renderer::drawFrame(const VulkanContext& ctx,
                          SwapChain&           swap,
                          const Pipeline&      pipeline,
                          const Mesh&          mesh,
                          const Material&      material,
                          const glm::mat4&     mvp)
{
    // ── Step 1: Wait for in-flight fence ──────────────────────────────────
    // Block the CPU until the GPU finishes all commands submitted in the
    // *previous* use of this frame slot.  This ensures I don't overwrite
    // the command buffer or signal a semaphore that the GPU is still reading.
    //
    // On frame 0, all fences start signalled, so this returns immediately.
    vkWaitForFences(ctx.device(), 1, &m_inFlightFences[m_currentFrame],
                    VK_TRUE, UINT64_MAX);

    // ── Step 2: Acquire next swapchain image ──────────────────────────────
    // Ask the swapchain for the index of an image I can draw into.
    // The call returns (nearly) immediately; the image may not yet be free.
    // imageAvailableSemaphores[currentFrame] is signalled by the driver
    // when the image is genuinely safe to write to.
    uint32_t imageIndex = 0;
    VkResult acquireResult = swap.acquireNextImage(
        ctx.device(),
        m_imageAvailableSemaphores[m_currentFrame],
        imageIndex);

    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
        // Window was resized between frames.  I haven't reset the fence yet
        // (I only reset it after a successful acquire below), so it stays
        // signalled and the next drawFrame() call on this slot won't block.
        return false;
    }
    if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
        spdlog::error("Renderer: vkAcquireNextImageKHR error {}", static_cast<int>(acquireResult));
        return false;
    }

    // ── Step 3: Reset fence ───────────────────────────────────────────────
    // Only reset the fence AFTER a successful acquire.  If I reset it before
    // and then the acquire failed, the fence would be unsignalled but no GPU
    // work is submitted to signal it again — the next vkWaitForFences would
    // block forever.
    vkResetFences(ctx.device(), 1, &m_inFlightFences[m_currentFrame]);

    // ── Step 4: Record command buffer ─────────────────────────────────────
    VkCommandBuffer cmd = m_commandBuffers[m_currentFrame];

    // Reset clears all previous commands and puts the buffer back into the
    // initial state, ready for a fresh vkBeginCommandBuffer.
    vkResetCommandBuffer(cmd, 0);

    // ONE_TIME_SUBMIT_BIT tells the driver this buffer will be submitted once
    // and reset before re-use.  This is a hint — the driver may optimise
    // accordingly (e.g. skip caching certain state).
    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    // ── Step 5: Barrier — UNDEFINED → COLOR_ATTACHMENT_OPTIMAL ───────────
    // I use the synchronization2 API (Vulkan 1.3 core).  Compared to the
    // original barrier API it offers finer-grained pipeline stage masks and
    // clearer semantics.
    //
    // oldLayout = UNDEFINED means "I don't care about the existing contents".
    // The driver is free to discard or overwrite the image data during the
    // transition — this is slightly faster than PRESERVE.
    //
    // srcStageMask = TOP_OF_PIPE means "nothing before this point needs to
    // finish" — there are no earlier writes to the image to wait for.
    //
    // dstStageMask = COLOR_ATTACHMENT_OUTPUT means "the colour write stage
    // must wait until this barrier completes" — ensuring the layout change
    // happens before the fragment shader tries to write colour data.
    VkImage swapImage = swap.images()[imageIndex];

    VkImageMemoryBarrier2 toRender{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    toRender.srcStageMask  = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
    toRender.srcAccessMask = VK_ACCESS_2_NONE;       // nothing to flush
    toRender.dstStageMask  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    toRender.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    toRender.oldLayout     = VK_IMAGE_LAYOUT_UNDEFINED;              // discard old contents
    toRender.newLayout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    toRender.image         = swapImage;
    toRender.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    VkDependencyInfo dep1{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    dep1.imageMemoryBarrierCount = 1;
    dep1.pImageMemoryBarriers    = &toRender;
    vkCmdPipelineBarrier2(cmd, &dep1);

    // ── Step 6: Begin Dynamic Rendering ───────────────────────────────────
    // vkCmdBeginRendering replaces vkCmdBeginRenderPass entirely.
    // I describe the colour attachment inline:
    //   loadOp  = CLEAR  → discard whatever was in the image and fill with clearValue
    //   storeOp = STORE  → keep the rendered result so the presentation engine can read it
    //
    // The clearValue of (0,0,0,1) gives a black background.
    VkClearValue clearBlack{};
    clearBlack.color = VkClearColorValue{{0.0f, 0.0f, 0.0f, 1.0f}};

    VkRenderingAttachmentInfo colourAttach{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    colourAttach.imageView   = swap.imageViews()[imageIndex]; // the specific image I acquired
    colourAttach.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colourAttach.loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colourAttach.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;
    colourAttach.clearValue  = clearBlack;

    VkRenderingInfo renderingInfo{VK_STRUCTURE_TYPE_RENDERING_INFO};
    renderingInfo.renderArea           = {{0, 0}, swap.extent()}; // full image, no offset
    renderingInfo.layerCount           = 1;                       // not a layered render
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments    = &colourAttach;
    // pDepthAttachment and pStencilAttachment are null — no depth buffer for M1

    vkCmdBeginRendering(cmd, &renderingInfo);

    // ── Step 7: Dynamic viewport and scissor ──────────────────────────────
    // I must set these every frame because they are dynamic pipeline state.
    // The viewport maps NDC (Normalised Device Coordinates, -1 to +1) to
    // pixel space.  The scissor is a pixel-space rectangle outside which
    // all fragments are discarded.  Both cover the whole swapchain image.
    VkViewport viewport{};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = static_cast<float>(swap.extent().width);
    viewport.height   = static_cast<float>(swap.extent().height);
    viewport.minDepth = 0.0f; // Vulkan depth range is [0,1] (GLM configured to match)
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{{0, 0}, swap.extent()};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // ── Step 8: Bind pipeline and draw ────────────────────────────────────
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.handle());
    material.bindDescriptorSet(cmd, pipeline.layout());
    vkCmdPushConstants(cmd,
                       pipeline.layout(),
                       VK_SHADER_STAGE_VERTEX_BIT,
                       0,
                       sizeof(glm::mat4),
                       &mvp);
    mesh.bind(cmd);
    mesh.draw(cmd);

    // ImGui draw calls are recorded inside the same Dynamic Rendering scope.
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

    // ── Step 9: End rendering ─────────────────────────────────────────────
    vkCmdEndRendering(cmd);

    // ── Step 10: Barrier — COLOR_ATTACHMENT_OPTIMAL → PRESENT_SRC_KHR ────
    // After rendering, the image must be transitioned to PRESENT_SRC_KHR so
    // the presentation engine can read it for display.
    //
    // srcStageMask = COLOR_ATTACHMENT_OUTPUT — wait for colour writes to finish.
    // dstStageMask = BOTTOM_OF_PIPE — the presentation engine runs after all
    //   GPU work; nothing on the GPU reads the image after this transition.
    // dstAccessMask = NONE — the presentation engine uses its own access path,
    //   not one visible through the Vulkan memory model.
    VkImageMemoryBarrier2 toPresent{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    toPresent.srcStageMask  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    toPresent.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    toPresent.dstStageMask  = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
    toPresent.dstAccessMask = VK_ACCESS_2_NONE;
    toPresent.oldLayout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    toPresent.newLayout     = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    toPresent.image         = swapImage;
    toPresent.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    VkDependencyInfo dep2{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    dep2.imageMemoryBarrierCount = 1;
    dep2.pImageMemoryBarriers    = &toPresent;
    vkCmdPipelineBarrier2(cmd, &dep2);

    // ── Step 11: End command buffer ───────────────────────────────────────
    // Finalises the recording.  After this, I cannot record into the buffer
    // again until it is reset.
    vkEndCommandBuffer(cmd);

    // ── Step 12: Submit ───────────────────────────────────────────────────
    // Submit the command buffer to the graphics queue.
    //
    // WAIT on imageAvailable[currentFrame]:
    //   The driver signals this semaphore when the acquired image is free.
    //   I wait at the COLOR_ATTACHMENT_OUTPUT stage — i.e. the GPU can run
    //   vertex shading immediately, and only stalls at the point where it
    //   would write colour data into the attachment.
    //
    // SIGNAL renderFinished[imageIndex]:
    //   Signalled when all GPU commands complete.  The presentation engine
    //   waits on this before displaying the image.  Indexed by imageIndex
    //   (not currentFrame) — see the file header for the safety reasoning.
    //
    // SIGNAL inFlightFences[currentFrame]:
    //   Signalled when all GPU commands complete.  The CPU waits on this
    //   at step 1 of the *next* use of this frame slot.
    VkSemaphore          waitSems[]   = {m_imageAvailableSemaphores[m_currentFrame]};
    VkSemaphore          signalSems[] = {m_renderFinishedSemaphores[imageIndex]};
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
    // Hand the finished image to the presentation engine.
    // It waits on renderFinished[imageIndex] before scanning out the image.
    VkSwapchainKHR   swapchains[] = {swap.handle()};
    VkPresentInfoKHR presentInfo{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = signalSems;
    presentInfo.swapchainCount     = 1;
    presentInfo.pSwapchains        = swapchains;
    presentInfo.pImageIndices      = &imageIndex;

    VkResult presentResult = vkQueuePresentKHR(ctx.graphicsQueue(), &presentInfo);

    // Advance the frame counter before handling the present result.
    // Both the success path and the rebuild path should use a different slot
    // on the next call.
    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

    // OUT_OF_DATE or SUBOPTIMAL both mean the swapchain no longer matches the
    // window — caller must rebuild before the next frame.
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR ||
        presentResult == VK_SUBOPTIMAL_KHR) {
        return false;
    }
    if (presentResult != VK_SUCCESS) {
        spdlog::error("Renderer: vkQueuePresentKHR error {}", static_cast<int>(presentResult));
        return false;
    }

    return true;
}

// =============================================================================
// waitIdle()
// =============================================================================

/// @details
/// `vkDeviceWaitIdle` blocks until all queues on the device have finished all
/// submitted work.  This is the "nuclear" synchronisation option — fine for
/// shutdown and swapchain rebuild, but too expensive for me to call every frame.
void Renderer::waitIdle(const VulkanContext& ctx)
{
    vkDeviceWaitIdle(ctx.device());
}
