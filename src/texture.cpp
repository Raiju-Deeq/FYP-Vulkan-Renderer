/**
 * @file texture.cpp
 * @brief Descriptor and sampler setup for the PBR textured mesh.
 */

#include "texture.hpp"

#include "vulkan_context.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <fstream>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace
{
using TextureResourceArray = std::array<GpuBuffer::ImageResource, PBR_TEXTURE_SLOT_COUNT>;

bool loadAsciiPpm(const std::string& path, LoadedTexture& outTexture)
{
    // This fallback lets me keep using tiny generated prototype textures even
    // when stb_image cannot identify the file.
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    std::string magic;
    int width = 0;
    int height = 0;
    int maxValue = 0;
    file >> magic >> width >> height >> maxValue;
    if (magic != "P3" || width <= 0 || height <= 0 || maxValue <= 0) {
        return false;
    }

    outTexture.pixels.clear();
    outTexture.pixels.reserve(static_cast<size_t>(width) * static_cast<size_t>(height) * 4);

    for (int i = 0; i < width * height; ++i) {
        int r = 0;
        int g = 0;
        int b = 0;
        file >> r >> g >> b;
        if (!file) {
            return false;
        }

        // I normalise whatever max value the PPM file uses into standard RGBA8.
        const auto scale = [maxValue](int value) {
            value = std::max(0, std::min(value, maxValue));
            return static_cast<uint8_t>((value * 255) / maxValue);
        };

        outTexture.pixels.push_back(scale(r));
        outTexture.pixels.push_back(scale(g));
        outTexture.pixels.push_back(scale(b));
        outTexture.pixels.push_back(255);
    }

    outTexture.width = width;
    outTexture.height = height;
    outTexture.channels = 4;
    outTexture.path = path;
    return true;
}

/**
 * @brief Creates a tiny fallback texture for an optional PBR map.
 *
 * This is what keeps the material path usable with only an OBJ and one base
 * colour PNG.  Every descriptor binding still receives a valid image, but the
 * fallback value is neutral:
 * - normal = flat normal colour (reserved until tangent-space normals exist);
 * - metallic/roughness = G=255 rough, B=0 non-metal;
 * - AO = white, so it does not darken the material;
 * - emissive = black, so it contributes no light unless I load an emissive map.
 *
 * @param rgba Four RGBA8 bytes used for the single texel.
 * @param label Debug path label shown in logs/RenderDoc.
 * @return LoadedTexture containing one RGBA texel.
 */
LoadedTexture makeFallbackTexture(const std::array<uint8_t, 4>& rgba,
                                  const std::string& label)
{
    LoadedTexture texture{};
    texture.pixels.assign(rgba.begin(), rgba.end());
    texture.width = 1;
    texture.height = 1;
    texture.channels = 4;
    texture.path = label;
    return texture;
}

/**
 * @brief Selects the real texture for a PBR slot or creates its neutral fallback.
 *
 * The returned reference is used immediately by Material::init() while
 * `fallback` is still alive in the caller's scope.  I use this pattern so I do
 * not have to permanently store fallback CPU textures in PbrTextureSet; they
 * only exist long enough to be uploaded into Vulkan images.
 *
 * @param textures CPU-side material texture set.
 * @param slot Material slot being uploaded.
 * @param fallback Temporary texture used when the slot is missing.
 * @return Texture data to upload for this slot.
 */
const LoadedTexture& textureForSlot(const PbrTextureSet& textures,
                                    PbrTextureSlot slot,
                                    LoadedTexture& fallback)
{
    switch (slot) {
    case PbrTextureSlot::BaseColor:
        return textures.baseColor;
    case PbrTextureSlot::Normal:
        if (textures.normal.has_value()) return textures.normal.value();
        fallback = makeFallbackTexture({128, 128, 255, 255}, "<default-flat-normal>");
        return fallback;
    case PbrTextureSlot::MetallicRoughness:
        if (textures.metallicRoughness.has_value()) return textures.metallicRoughness.value();
        fallback = makeFallbackTexture({255, 255, 0, 255}, "<default-metallic-roughness>");
        return fallback;
    case PbrTextureSlot::AmbientOcclusion:
        if (textures.ambientOcclusion.has_value()) return textures.ambientOcclusion.value();
        fallback = makeFallbackTexture({255, 255, 255, 255}, "<default-ao>");
        return fallback;
    case PbrTextureSlot::Emissive:
        if (textures.emissive.has_value()) return textures.emissive.value();
        fallback = makeFallbackTexture({0, 0, 0, 255}, "<default-emissive>");
        return fallback;
    }

    return textures.baseColor;
}

/**
 * @brief Chooses the Vulkan image format for a PBR texture slot.
 *
 * Colour textures must be sampled through sRGB conversion because their bytes
 * represent display colour.  Data textures must not be gamma-corrected, because
 * values like roughness and metallic are numeric material parameters.
 *
 * @param slot Material texture slot.
 * @return Vulkan format used when creating the image.
 */
VkFormat formatForSlot(PbrTextureSlot slot)
{
    // Colour maps are sRGB because their values represent visible colour.
    // Data maps are UNORM because values like roughness, metallic, AO and
    // normal vectors must be sampled linearly without gamma conversion.
    if (slot == PbrTextureSlot::BaseColor || slot == PbrTextureSlot::Emissive) {
        return VK_FORMAT_R8G8B8A8_SRGB;
    }
    return VK_FORMAT_R8G8B8A8_UNORM;
}

/**
 * @brief Destroys every image in a temporary or owned material texture array.
 * @param ctx Vulkan context that owns the VMA allocator and VkDevice.
 * @param resources Texture resources to destroy/reset.
 */
void destroyTextureArray(const VulkanContext& ctx, TextureResourceArray& resources)
{
    for (GpuBuffer::ImageResource& resource : resources) {
        GpuBuffer::destroyImage(ctx, resource);
    }
}
} // namespace

bool loadRgba8Texture(const std::string& path, LoadedTexture& outTexture)
{
    int width = 0;
    int height = 0;
    int sourceChannels = 0;

    // Force 4-channel RGBA8 output regardless of source format.  I keep the
    // CPU byte layout identical for every texture, then choose the Vulkan image
    // format later: sRGB for colour maps, UNORM for data maps.
    stbi_uc* pixels = stbi_load(path.c_str(), &width, &height, &sourceChannels, STBI_rgb_alpha);
    if (!pixels) {
        if (loadAsciiPpm(path, outTexture)) {
            spdlog::info("TextureLoader: loaded '{}' ({}x{}, ASCII PPM)",
                         path, outTexture.width, outTexture.height);
            return true;
        }

        spdlog::error("TextureLoader: failed to load '{}': {}", path, stbi_failure_reason());
        return false;
    }

    const size_t byteCount = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
    outTexture.pixels.assign(pixels, pixels + byteCount);
    outTexture.width = width;
    outTexture.height = height;
    outTexture.channels = 4;
    outTexture.path = path;

    stbi_image_free(pixels);

    spdlog::info("TextureLoader: loaded '{}' ({}x{}, source channels={})",
                 path, width, height, sourceChannels);
    return true;
}

bool Material::init(const VulkanContext&              ctx,
                    const PbrTextureSet& textures,
                    VkDescriptorSetLayout             layout)
{
    // Build replacement state in local variables first.  If any step fails,
    // the currently bound material still owns its previous valid GPU objects.
    TextureResourceArray newTextures{};
    VkSampler newSampler = VK_NULL_HANDLE;
    VkDescriptorPool newDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet newDescriptorSet = VK_NULL_HANDLE;
    uint32_t maxMipLevels = 1;

    for (uint32_t i = 0; i < PBR_TEXTURE_SLOT_COUNT; ++i) {
        const auto slot = static_cast<PbrTextureSlot>(i);
        LoadedTexture fallback{};
        const LoadedTexture& source = textureForSlot(textures, slot, fallback);

        if (!GpuBuffer::uploadTexture2D(ctx, source, newTextures[i], formatForSlot(slot))) {
            spdlog::error("Material: failed to upload PBR texture slot {} from '{}'",
                          i, source.path);
            destroyTextureArray(ctx, newTextures);
            return false;
        }
        maxMipLevels = std::max(maxMipLevels, newTextures[i].mipLevels);
    }

    VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    // Mipmapping is controlled from the sampler as well as the image.  The
    // image view exposes every generated mip level, and this sampler tells the
    // GPU it may pick from that full range when a textured triangle is far
    // enough away on screen.  That reduces shimmering because the shader reads
    // a pre-filtered smaller texture instead of over-sampling the full image.
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = static_cast<float>(maxMipLevels);
    samplerInfo.maxAnisotropy = 1.0f;

    if (vkCreateSampler(ctx.device(), &samplerInfo, nullptr, &newSampler) != VK_SUCCESS) {
        spdlog::error("Material: failed to create sampler");
        destroyTextureArray(ctx, newTextures);
        return false;
    }

    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = PBR_TEXTURE_SLOT_COUNT;

    VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;

    if (vkCreateDescriptorPool(ctx.device(), &poolInfo, nullptr, &newDescriptorPool) != VK_SUCCESS) {
        spdlog::error("Material: failed to create descriptor pool");
        vkDestroySampler(ctx.device(), newSampler, nullptr);
        destroyTextureArray(ctx, newTextures);
        return false;
    }

    VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocInfo.descriptorPool = newDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    if (vkAllocateDescriptorSets(ctx.device(), &allocInfo, &newDescriptorSet) != VK_SUCCESS) {
        spdlog::error("Material: failed to allocate descriptor set");
        vkDestroyDescriptorPool(ctx.device(), newDescriptorPool, nullptr);
        vkDestroySampler(ctx.device(), newSampler, nullptr);
        destroyTextureArray(ctx, newTextures);
        return false;
    }

    std::array<VkDescriptorImageInfo, PBR_TEXTURE_SLOT_COUNT> imageInfos{};
    std::array<VkWriteDescriptorSet, PBR_TEXTURE_SLOT_COUNT> writes{};
    for (uint32_t i = 0; i < PBR_TEXTURE_SLOT_COUNT; ++i) {
        imageInfos[i].sampler = newSampler;
        imageInfos[i].imageView = newTextures[i].view;
        imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        // Binding numbers intentionally match PbrTextureSlot and mesh.frag.
        writes[i] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        writes[i].dstSet = newDescriptorSet;
        writes[i].dstBinding = i;
        writes[i].descriptorCount = 1;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[i].pImageInfo = &imageInfos[i];
    }

    vkUpdateDescriptorSets(ctx.device(),
                           static_cast<uint32_t>(writes.size()),
                           writes.data(),
                           0,
                           nullptr);

    // Only replace the old material after the new texture state is complete.
    destroy(ctx);
    m_textures = newTextures;
    m_sampler = newSampler;
    m_descriptorPool = newDescriptorPool;
    m_descriptorSet = newDescriptorSet;

    spdlog::info("Material: PBR descriptor set ready for '{}'", textures.baseColor.path);
    return true;
}

void Material::destroy(const VulkanContext& ctx)
{
    // Descriptor sets are freed with their pool, so I only need to destroy the
    // pool handle explicitly.
    if (m_descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(ctx.device(), m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
        m_descriptorSet = VK_NULL_HANDLE;
    }

    if (m_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(ctx.device(), m_sampler, nullptr);
        m_sampler = VK_NULL_HANDLE;
    }

    destroyTextureArray(ctx, m_textures);
}

void Material::bindDescriptorSet(VkCommandBuffer  cmd,
                                 VkPipelineLayout layout) const
{
    vkCmdBindDescriptorSets(cmd,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            layout,
                            0,
                            1,
                            &m_descriptorSet,
                            0,
                            nullptr);
}
