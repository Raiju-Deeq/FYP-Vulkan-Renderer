/**
 * @file texture.hpp
 * @brief One sampled albedo texture and descriptor set for the M2 mesh path.
 */

#ifndef FYP_VULKAN_RENDERER_TEXTURE_HPP
#define FYP_VULKAN_RENDERER_TEXTURE_HPP

#include "gpu_buffer.hpp"

#include <vulkan/vulkan.h>
#include <cstdint>
#include <string>
#include <vector>

class VulkanContext;

/**
 * @struct LoadedTexture
 * @brief CPU-side RGBA8 texture data returned by stb_image.
 *
 * Pixels are always converted to 4 channels so the Vulkan upload path can use
 * one predictable format: `VK_FORMAT_R8G8B8A8_SRGB`.
 */
struct LoadedTexture
{
    std::vector<uint8_t> pixels;   ///< Tightly packed RGBA8 pixels.
    int                 width = 0; ///< Width in pixels.
    int                 height = 0; ///< Height in pixels.
    int                 channels = 4; ///< Always 4 after loading.
    std::string         path;      ///< Source path, useful for logging/debug UI.
};

/**
 * @brief Loads an image file as tightly packed RGBA8 pixels.
 *
 * Uses stb_image for common texture formats and keeps the original ASCII PPM
 * fallback so generated prototype assets still work.
 *
 * @param path Filesystem path to the source texture.
 * @param outTexture Receives converted RGBA8 pixels and metadata.
 * @return true if loading succeeds, false otherwise.
 */
bool loadRgba8Texture(const std::string& path, LoadedTexture& outTexture);

/**
 * @class Material
 * @brief Owns the sampled albedo texture, sampler, descriptor pool and descriptor set.
 */
class Material
{
public:
    /**
     * @brief Uploads texture pixels to the GPU and creates the descriptor set.
     * @param ctx Initialised Vulkan context.
     * @param texture CPU-side RGBA8 texture data.
     * @param layout Descriptor set layout from Pipeline.
     * @return true on success, false if any Vulkan/VMA allocation fails.
     */
    bool init(const VulkanContext&             ctx,
              const LoadedTexture&            texture,
              VkDescriptorSetLayout           layout);

    /**
     * @brief Destroys all Vulkan handles owned by this material.
     * @param ctx Context that owns the VkDevice and VMA allocator.
     */
    void destroy(const VulkanContext& ctx);

    /**
     * @brief Binds the material descriptor set for a draw call.
     * @param cmd Command buffer currently being recorded.
     * @param layout Pipeline layout compatible with the descriptor set.
     */
    void bindDescriptorSet(VkCommandBuffer  cmd,
                           VkPipelineLayout layout) const;

private:
    GpuBuffer::ImageResource m_albedo{};
    VkSampler                  m_sampler = VK_NULL_HANDLE;
    VkDescriptorPool           m_descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet            m_descriptorSet = VK_NULL_HANDLE;
};

#endif // FYP_VULKAN_RENDERER_TEXTURE_HPP
