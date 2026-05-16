/**
 * @file texture.hpp
 * @brief Texture loading plus the sampled material descriptor used by meshes.
 */

#ifndef FYP_VULKAN_RENDERER_TEXTURE_HPP
#define FYP_VULKAN_RENDERER_TEXTURE_HPP

#include "gpu_buffer.hpp"

#include <vulkan/vulkan.h>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

class VulkanContext;

/**
 * @struct LoadedTexture
 * @brief CPU-side RGBA8 texture data returned by stb_image.
 *
 * Pixels are always converted to 4 channels so the upload code can use one
 * byte layout for every map.  The Vulkan image format is chosen at upload time:
 * colour maps use sRGB, while data maps such as AO/roughness/metallic use UNORM.
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
 * @enum PbrTextureSlot
 * @brief Texture bindings used by the simple PBR material.
 */
enum class PbrTextureSlot : uint32_t
{
    BaseColor = 0,          ///< sRGB colour map used as the material base colour.
    Normal = 1,             ///< Linear normal-map upload slot; shading uses vertex normals until tangents exist.
    MetallicRoughness = 2,  ///< Linear packed map: G = roughness, B = metallic.
    AmbientOcclusion = 3,   ///< Linear occlusion map, sampled from the red channel.
    Emissive = 4            ///< sRGB emissive colour map.
};

static constexpr uint32_t PBR_TEXTURE_SLOT_COUNT = 5;

/**
 * @struct PbrTextureSet
 * @brief CPU-side texture set used to initialise a PBR material.
 *
 * Only baseColor needs to come from a real asset for the renderer to work.
 * Missing optional maps are replaced with tiny 1x1 defaults in Material::init().
 */
struct PbrTextureSet
{
    LoadedTexture baseColor;                         ///< Required visible colour texture.
    std::optional<LoadedTexture> normal;             ///< Optional normal map.
    std::optional<LoadedTexture> metallicRoughness;  ///< Optional packed roughness/metallic map.
    std::optional<LoadedTexture> ambientOcclusion;   ///< Optional AO map.
    std::optional<LoadedTexture> emissive;           ///< Optional emissive map.
};

/**
 * @class Material
 * @brief Owns the sampled PBR textures, sampler, descriptor pool and descriptor set.
 */
class Material
{
public:
    /**
     * @brief Uploads texture pixels to the GPU and creates the descriptor set.
     *
     * Re-initialisation is non-destructive until the new texture, sampler and
     * descriptor set are all ready.  That way a failed runtime texture reload
     * leaves the currently displayed material intact.
     *
     * @param ctx Initialised Vulkan context.
     * @param textures CPU-side PBR texture set. Only baseColor is required.
     * @param layout Descriptor set layout from Pipeline.
     * @return true on success, false if any Vulkan/VMA allocation fails.
     */
    bool init(const VulkanContext&             ctx,
              const PbrTextureSet&            textures,
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
    std::array<GpuBuffer::ImageResource, PBR_TEXTURE_SLOT_COUNT> m_textures{};
    VkSampler                  m_sampler = VK_NULL_HANDLE;
    VkDescriptorPool           m_descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet            m_descriptorSet = VK_NULL_HANDLE;
};

#endif // FYP_VULKAN_RENDERER_TEXTURE_HPP
