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
                     VkAccessFlags2  dstAccess)
{
    VkImageMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    barrier.srcStageMask = srcStage;
    barrier.srcAccessMask = srcAccess;
    barrier.dstStageMask = dstStage;
    barrier.dstAccessMask = dstAccess;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.image = image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    VkDependencyInfo dep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    dep.imageMemoryBarrierCount = 1;
    dep.pImageMemoryBarriers = &barrier;
    vkCmdPipelineBarrier2(cmd, &dep);
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
                     ImageResource&       outImage)
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

    outImage.format = VK_FORMAT_R8G8B8A8_SRGB;
    outImage.extent = {
        static_cast<uint32_t>(texture.width),
        static_cast<uint32_t>(texture.height),
        1
    };

    VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = outImage.format;
    imageInfo.extent = outImage.extent;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
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
                    VK_ACCESS_2_TRANSFER_WRITE_BIT);

    // Once the image is in TRANSFER_DST, I can copy the staged RGBA pixels into it.
    VkBufferImageCopy copy{};
    copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy.imageExtent = outImage.extent;
    vkCmdCopyBufferToImage(cmd, staging.buffer, outImage.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

    transitionImage(cmd, outImage.image,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                    VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                    VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);

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
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

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
}
} // namespace GpuBuffer
