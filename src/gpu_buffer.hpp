/**
 * @file gpu_buffer.hpp
 * @brief Vulkan/VMA staging helpers for mesh and texture uploads.
 *
 * This module keeps the repeated staging-buffer upload code in one place.
 */

#ifndef FYP_VULKAN_RENDERER_GPU_BUFFER_HPP
#define FYP_VULKAN_RENDERER_GPU_BUFFER_HPP

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

class VulkanContext;
struct LoadedTexture;

namespace GpuBuffer
{
/**
 * @struct BufferResource
 * @brief VkBuffer plus the VMA allocation that backs it.
 */
struct BufferResource
{
    VkBuffer      buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkDeviceSize  size = 0;
};

/**
 * @struct ImageResource
 * @brief VkImage plus allocation and view for a sampled 2D texture.
 */
struct ImageResource
{
    VkImage       image = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkImageView   view = VK_NULL_HANDLE;
    VkFormat      format = VK_FORMAT_UNDEFINED;
    VkExtent3D    extent = {0, 0, 1};
};

/**
 * @brief Uploads arbitrary bytes into a device-local buffer.
 *
 * Mesh uses this for vertex and index buffers:
 *  - vertices: `VK_BUFFER_USAGE_VERTEX_BUFFER_BIT`
 *  - indices:  `VK_BUFFER_USAGE_INDEX_BUFFER_BIT`
 *
 * The function adds `VK_BUFFER_USAGE_TRANSFER_DST_BIT` internally because the
 * final buffer receives data from a staging buffer.
 */
bool uploadBuffer(const VulkanContext& ctx,
                  const void*          data,
                  VkDeviceSize         byteSize,
                  VkBufferUsageFlags   usage,
                  BufferResource&      outBuffer);

/**
 * @brief Uploads an RGBA8 texture into a sampled device-local image.
 *
 * Creates the image, stages the pixels, transitions layouts, copies the pixel
 * data, transitions to `SHADER_READ_ONLY_OPTIMAL`, and creates an image view.
 */
bool uploadTexture2D(const VulkanContext& ctx,
                     const LoadedTexture& texture,
                     ImageResource&       outImage);

/// @brief Destroys a BufferResource created by uploadBuffer().
void destroyBuffer(const VulkanContext& ctx, BufferResource& buffer);

/// @brief Destroys an ImageResource created by uploadTexture2D().
void destroyImage(const VulkanContext& ctx, ImageResource& image);
} // namespace GpuBuffer

#endif // FYP_VULKAN_RENDERER_GPU_BUFFER_HPP
