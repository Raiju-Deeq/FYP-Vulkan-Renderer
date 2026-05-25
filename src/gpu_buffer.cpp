/**
 * @file gpu_buffer.cpp
 * @brief Vulkan/VMA staging implementation for renderer resources.
 */

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include "vulkan_context.hpp"
#include "gpu_buffer.hpp"
#include "texture.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstring>

namespace GpuBuffer
{
namespace
{
bool createBuffer(const VulkanContext& ctx,
                  VkDeviceSize         size,
                  VkBufferUsageFlags   usage,
                  VmaAllocationCreateFlags allocFlags,
                  VmaMemoryUsage       memoryUsage,
                  BufferResource&      outBuffer)
{
    VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.flags = allocFlags;
    allocInfo.usage = memoryUsage;

    // I create buffers through VMA so the rest of the renderer never has to
    // choose memory heaps or call vkAllocateMemory directly.
    if (vmaCreateBuffer(ctx.allocator(), &bufferInfo, &allocInfo,
                        &outBuffer.buffer, &outBuffer.allocation, nullptr) != VK_SUCCESS) {
        return false;
    }

    outBuffer.size = size;
    return true;
}

bool beginSingleUseCommands(const VulkanContext& ctx,
                            VkCommandPool&      outPool,
                            VkCommandBuffer&    outCmd)
{
    // TRANSIENT_BIT hints to the driver that this pool's command buffers are
    // short-lived (submit once then discard) — may enable allocation optimisations.
    VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    poolInfo.queueFamilyIndex = ctx.graphicsQueueFamily();

    if (vkCreateCommandPool(ctx.device(), &poolInfo, nullptr, &outPool) != VK_SUCCESS) {
        return false;
    }

    VkCommandBufferAllocateInfo allocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocInfo.commandPool = outPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(ctx.device(), &allocInfo, &outCmd) != VK_SUCCESS) {
        vkDestroyCommandPool(ctx.device(), outPool, nullptr);
        outPool = VK_NULL_HANDLE;
        return false;
    }

    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(outCmd, &beginInfo) != VK_SUCCESS) {
        vkDestroyCommandPool(ctx.device(), outPool, nullptr);
        outPool = VK_NULL_HANDLE;
        outCmd = VK_NULL_HANDLE;
        return false;
    }

    return true;
}

bool endSingleUseCommands(const VulkanContext& ctx,
                          VkCommandPool       pool,
                          VkCommandBuffer     cmd)
{
    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
        vkDestroyCommandPool(ctx.device(), pool, nullptr);
        return false;
    }

    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    const VkResult submitResult = vkQueueSubmit(ctx.graphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
    if (submitResult == VK_SUCCESS) {
        // vkQueueWaitIdle is fine here because uploads are one-off setup operations,
        // not per-frame work.  A fence would be more efficient for repeated transfers.
        vkQueueWaitIdle(ctx.graphicsQueue());
    }

    vkDestroyCommandPool(ctx.device(), pool, nullptr);
    return submitResult == VK_SUCCESS;
}

void transitionImage(VkCommandBuffer cmd,
                     VkImage         image,
                     VkImageLayout   oldLayout,
                     VkImageLayout   newLayout,
                     VkPipelineStageFlags2 srcStage,
                     VkAccessFlags2  srcAccess,
                     VkPipelineStageFlags2 dstStage,
                     VkAccessFlags2  dstAccess,
                     uint32_t        baseMipLevel = 0,
                     uint32_t        levelCount = 1)
{
    VkImageMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    barrier.srcStageMask = srcStage;
    barrier.srcAccessMask = srcAccess;
    barrier.dstStageMask = dstStage;
    barrier.dstAccessMask = dstAccess;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.image = image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, baseMipLevel, levelCount, 0, 1};

    VkDependencyInfo dep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    dep.imageMemoryBarrierCount = 1;
    dep.pImageMemoryBarriers = &barrier;
    vkCmdPipelineBarrier2(cmd, &dep);
}

/**
 * @brief Calculates how many mip levels a 2D texture needs.
 *
 * A mip chain keeps halving the largest dimension until both width and height
 * reach 1.  For example, a 1024x512 texture becomes:
 * 1024x512, 512x256, 256x128 ... 2x1, 1x1.
 *
 * @param width Texture width in pixels.
 * @param height Texture height in pixels.
 * @return Number of levels required for a full mip chain.
 */
uint32_t calculateMipLevels(uint32_t width, uint32_t height)
{
    uint32_t levels = 1;
    while (width > 1 || height > 1) {
        width = std::max(1u, width / 2u);
        height = std::max(1u, height / 2u);
        ++levels;
    }
    return levels;
}

/**
 * @brief Checks whether this texture format can generate mipmaps on the GPU.
 *
 * I generate mipmaps with `vkCmdBlitImage` and linear filtering.  That means
 * the format must support blit source, blit destination, and linear filtering
 * when used with optimal tiling.  If the chosen format fails this check, I stop
 * before recording invalid blit commands.
 *
 * @param ctx Vulkan context used to query physical-device format support.
 * @param format Image format that will be blitted between mip levels.
 * @return true if the format can be used for GPU mip generation.
 */
bool supportsLinearMipBlit(const VulkanContext& ctx, VkFormat format)
{
    VkFormatProperties properties{};
    vkGetPhysicalDeviceFormatProperties(ctx.physicalDevice(), format, &properties);

    const VkFormatFeatureFlags required =
        VK_FORMAT_FEATURE_BLIT_SRC_BIT |
        VK_FORMAT_FEATURE_BLIT_DST_BIT |
        VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;

    return (properties.optimalTilingFeatures & required) == required;
}

/**
 * @brief Records the barriers and blits that build a complete mip chain.
 *
 * The base level has already been filled from the staging buffer before this
 * function runs.  From there the process repeats:
 *
 * 1. Turn the previous level into `TRANSFER_SRC_OPTIMAL`.
 * 2. Blit it into the next smaller level, which is still `TRANSFER_DST_OPTIMAL`.
 * 3. Turn the previous level into `SHADER_READ_ONLY_OPTIMAL`.
 *
 * At the end, the final 1x1 level has no smaller destination, so I transition
 * that last level from transfer destination straight to shader-read.
 *
 * @param cmd Command buffer currently recording the one-time upload work.
 * @param image Texture image containing all mip levels.
 * @param width Base mip width in pixels.
 * @param height Base mip height in pixels.
 * @param mipLevels Number of levels in the image.
 */
void generateMipmaps(VkCommandBuffer cmd,
                     VkImage         image,
                     uint32_t        width,
                     uint32_t        height,
                     uint32_t        mipLevels)
{
    uint32_t mipWidth = width;
    uint32_t mipHeight = height;

    for (uint32_t level = 1; level < mipLevels; ++level) {
        const uint32_t previousLevel = level - 1;

        // The previous level has finished receiving pixels, either from the
        // original staging copy (mip 0) or from the previous blit.  I now make
        // it the source image for the next downsample operation.
        transitionImage(cmd, image,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                        VK_ACCESS_2_TRANSFER_WRITE_BIT,
                        VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                        VK_ACCESS_2_TRANSFER_READ_BIT,
                        previousLevel,
                        1);

        const uint32_t nextWidth = std::max(1u, mipWidth / 2u);
        const uint32_t nextHeight = std::max(1u, mipHeight / 2u);

        VkImageBlit blit{};
        blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, previousLevel, 0, 1};
        blit.srcOffsets[0] = {0, 0, 0};
        blit.srcOffsets[1] = {
            static_cast<int32_t>(mipWidth),
            static_cast<int32_t>(mipHeight),
            1
        };
        blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, level, 0, 1};
        blit.dstOffsets[0] = {0, 0, 0};
        blit.dstOffsets[1] = {
            static_cast<int32_t>(nextWidth),
            static_cast<int32_t>(nextHeight),
            1
        };

        // Linear filtering is what makes this a useful mipmap instead of a
        // hard nearest-neighbour shrink.  The format support check happens
        // before this command buffer is recorded.
        vkCmdBlitImage(cmd,
                       image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       1, &blit,
                       VK_FILTER_LINEAR);

        // This level is now complete.  I transition it to shader-read
        // immediately so no later command accidentally writes to it again.
        transitionImage(cmd, image,
                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                        VK_ACCESS_2_TRANSFER_READ_BIT,
                        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                        VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                        previousLevel,
                        1);

        mipWidth = nextWidth;
        mipHeight = nextHeight;
    }

    // The last level was only ever written as a transfer destination.  There
    // is no smaller level to blit from it, so I only need the final shader-read
    // transition.  If mipLevels is 1, this correctly transitions level 0.
    transitionImage(cmd, image,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                    VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                    VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                    mipLevels - 1,
                    1);
}
} // namespace

bool uploadBuffer(const VulkanContext& ctx,
                  const void*          data,
                  VkDeviceSize         byteSize,
                  VkBufferUsageFlags   usage,
                  BufferResource&      outBuffer)
{
    if (data == nullptr || byteSize == 0) {
        spdlog::error("GpuBuffer: uploadBuffer called with no data");
        return false;
    }

    // HOST_ACCESS_SEQUENTIAL_WRITE + VMA_MEMORY_USAGE_AUTO = VMA picks the most
    // efficient host-visible heap (usually CPU-side BAR on discrete GPUs).
    BufferResource staging{};
    if (!createBuffer(ctx, byteSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                      VMA_MEMORY_USAGE_AUTO, staging)) {
        spdlog::error("GpuBuffer: failed to create staging buffer");
        return false;
    }

    void* mapped = nullptr;
    if (vmaMapMemory(ctx.allocator(), staging.allocation, &mapped) != VK_SUCCESS) {
        spdlog::error("GpuBuffer: failed to map staging buffer");
        destroyBuffer(ctx, staging);
        return false;
    }
    // I write to a CPU-visible staging buffer first, then copy into fast
    // device-local memory with a one-time command buffer.
    std::memcpy(mapped, data, static_cast<size_t>(byteSize));
    vmaUnmapMemory(ctx.allocator(), staging.allocation);

    if (!createBuffer(ctx, byteSize, usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                      0, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, outBuffer)) {
        spdlog::error("GpuBuffer: failed to create device buffer");
        destroyBuffer(ctx, staging);
        return false;
    }

    VkCommandPool pool = VK_NULL_HANDLE;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    if (!beginSingleUseCommands(ctx, pool, cmd)) {
        spdlog::error("GpuBuffer: failed to begin copy command buffer");
        destroyBuffer(ctx, staging);
        destroyBuffer(ctx, outBuffer);
        return false;
    }

    VkBufferCopy copy{};
    copy.size = byteSize;
    vkCmdCopyBuffer(cmd, staging.buffer, outBuffer.buffer, 1, &copy);

    const bool submitted = endSingleUseCommands(ctx, pool, cmd);
    destroyBuffer(ctx, staging);

    if (!submitted) {
        spdlog::error("GpuBuffer: buffer copy submit failed");
        destroyBuffer(ctx, outBuffer);
        return false;
    }

    return true;
}

bool uploadTexture2D(const VulkanContext& ctx,
                     const LoadedTexture& texture,
                     ImageResource&       outImage,
                     VkFormat             format)
{
    if (texture.pixels.empty() || texture.width <= 0 || texture.height <= 0) {
        spdlog::error("GpuBuffer: uploadTexture2D called with invalid texture");
        return false;
    }

    const VkDeviceSize byteSize = static_cast<VkDeviceSize>(texture.pixels.size());

    BufferResource staging{};
    if (!createBuffer(ctx, byteSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                      VMA_MEMORY_USAGE_AUTO, staging)) {
        spdlog::error("GpuBuffer: failed to create texture staging buffer");
        return false;
    }

    void* mapped = nullptr;
    if (vmaMapMemory(ctx.allocator(), staging.allocation, &mapped) != VK_SUCCESS) {
        spdlog::error("GpuBuffer: failed to map texture staging buffer");
        destroyBuffer(ctx, staging);
        return false;
    }
    std::memcpy(mapped, texture.pixels.data(), texture.pixels.size());
    vmaUnmapMemory(ctx.allocator(), staging.allocation);

    outImage.format = format;
    outImage.extent = {
        static_cast<uint32_t>(texture.width),
        static_cast<uint32_t>(texture.height),
        1
    };
    outImage.mipLevels = calculateMipLevels(outImage.extent.width, outImage.extent.height);

    if (!supportsLinearMipBlit(ctx, outImage.format)) {
        spdlog::error("GpuBuffer: format {} does not support linear mipmap generation",
                      static_cast<int>(outImage.format));
        destroyBuffer(ctx, staging);
        return false;
    }

    VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = outImage.format;
    imageInfo.extent = outImage.extent;
    imageInfo.mipLevels = outImage.mipLevels;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage =
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
        VK_IMAGE_USAGE_TRANSFER_DST_BIT |
        VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    if (vmaCreateImage(ctx.allocator(), &imageInfo, &allocInfo,
                       &outImage.image, &outImage.allocation, nullptr) != VK_SUCCESS) {
        spdlog::error("GpuBuffer: failed to create texture image");
        destroyBuffer(ctx, staging);
        return false;
    }

    VkCommandPool pool = VK_NULL_HANDLE;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    if (!beginSingleUseCommands(ctx, pool, cmd)) {
        spdlog::error("GpuBuffer: failed to begin texture upload command buffer");
        destroyBuffer(ctx, staging);
        destroyImage(ctx, outImage);
        return false;
    }

    transitionImage(cmd, outImage.image,
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                    VK_ACCESS_2_NONE,
                    VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                    VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    0,
                    outImage.mipLevels);

    // Once the image is in TRANSFER_DST, I can copy the staged RGBA pixels into it.
    VkBufferImageCopy copy{};
    copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy.imageExtent = outImage.extent;
    vkCmdCopyBufferToImage(cmd, staging.buffer, outImage.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

    // The base level is now uploaded.  I generate the lower-resolution mip
    // levels on the GPU so the CPU loader does not need to resize pixels.
    generateMipmaps(cmd,
                    outImage.image,
                    outImage.extent.width,
                    outImage.extent.height,
                    outImage.mipLevels);

    // After this point the fragment shader can sample the image safely.
    const bool submitted = endSingleUseCommands(ctx, pool, cmd);
    destroyBuffer(ctx, staging);
    if (!submitted) {
        spdlog::error("GpuBuffer: texture upload submit failed");
        destroyImage(ctx, outImage);
        return false;
    }

    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = outImage.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = outImage.format;
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, outImage.mipLevels, 0, 1};

    if (vkCreateImageView(ctx.device(), &viewInfo, nullptr, &outImage.view) != VK_SUCCESS) {
        spdlog::error("GpuBuffer: failed to create texture image view");
        destroyImage(ctx, outImage);
        return false;
    }

    return true;
}

void destroyBuffer(const VulkanContext& ctx, BufferResource& buffer)
{
    if (buffer.buffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(ctx.allocator(), buffer.buffer, buffer.allocation);
        buffer.buffer = VK_NULL_HANDLE;
        buffer.allocation = VK_NULL_HANDLE;
        buffer.size = 0;
    }
}

void destroyImage(const VulkanContext& ctx, ImageResource& image)
{
    if (image.view != VK_NULL_HANDLE) {
        vkDestroyImageView(ctx.device(), image.view, nullptr);
        image.view = VK_NULL_HANDLE;
    }

    if (image.image != VK_NULL_HANDLE) {
        vmaDestroyImage(ctx.allocator(), image.image, image.allocation);
        image.image = VK_NULL_HANDLE;
        image.allocation = VK_NULL_HANDLE;
    }

    image.format = VK_FORMAT_UNDEFINED;
    image.extent = {0, 0, 1};
    image.mipLevels = 1;
}
} // namespace GpuBuffer
