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
 * @brief VkImage plus allocation, view and mip metadata for a sampled 2D texture.
 */
struct ImageResource
{
    VkImage       image = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkImageView   view = VK_NULL_HANDLE;
    VkFormat      format = VK_FORMAT_UNDEFINED;
    VkExtent3D    extent = {0, 0, 1};
    uint32_t      mipLevels = 1;
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
 * @brief Uploads RGBA8 pixels into a sampled device-local image.
 *
 * Creates the image, stages the base pixels into mip level 0, generates the
 * remaining mip levels with GPU blits, transitions the whole mip chain to
 * `SHADER_READ_ONLY_OPTIMAL`, and creates an image view that exposes all levels.
 *
 * @param ctx Initialised Vulkan context.
 * @param texture CPU-side RGBA8 pixels.
 * @param outImage Receives the created Vulkan image, view, allocation and mip count.
 * @param format Vulkan image format. Use sRGB for colour maps and UNORM for data maps.
 */
bool uploadTexture2D(const VulkanContext& ctx,
                     const LoadedTexture& texture,
                     ImageResource&       outImage,
                     VkFormat             format = VK_FORMAT_R8G8B8A8_SRGB);

/// @brief Destroys a BufferResource created by uploadBuffer().
void destroyBuffer(const VulkanContext& ctx, BufferResource& buffer);

/// @brief Destroys an ImageResource created by uploadTexture2D().
void destroyImage(const VulkanContext& ctx, ImageResource& image);
} // namespace GpuBuffer

#endif // FYP_VULKAN_RENDERER_GPU_BUFFER_HPP
