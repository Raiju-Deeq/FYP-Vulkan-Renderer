/**
 * @file texture.cpp
 * @brief Descriptor and sampler setup for the M2 textured mesh.
 */

#include "texture.hpp"

#include "vulkan_context.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <fstream>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace
{
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
} // namespace

bool loadRgba8Texture(const std::string& path, LoadedTexture& outTexture)
{
    int width = 0;
    int height = 0;
    int sourceChannels = 0;

    // Force 4-channel RGBA8 output regardless of source format so the Vulkan
    // upload path can use VK_FORMAT_R8G8B8A8_SRGB without per-texture branching.
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
                    const LoadedTexture& texture,
                    VkDescriptorSetLayout             layout)
{
    // Re-initialising a material should replace all GPU-side texture state.
    destroy(ctx);

    if (!GpuBuffer::uploadTexture2D(ctx, texture, m_albedo)) {
        spdlog::error("Material: failed to upload albedo texture");
        return false;
    }

    VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;
    samplerInfo.maxAnisotropy = 1.0f;

    if (vkCreateSampler(ctx.device(), &samplerInfo, nullptr, &m_sampler) != VK_SUCCESS) {
        spdlog::error("Material: failed to create sampler");
        destroy(ctx);
        return false;
    }

    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;

    if (vkCreateDescriptorPool(ctx.device(), &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
        spdlog::error("Material: failed to create descriptor pool");
        destroy(ctx);
        return false;
    }

    VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    if (vkAllocateDescriptorSets(ctx.device(), &allocInfo, &m_descriptorSet) != VK_SUCCESS) {
        spdlog::error("Material: failed to allocate descriptor set");
        destroy(ctx);
        return false;
    }

    VkDescriptorImageInfo imageInfo{};
    imageInfo.sampler = m_sampler;
    imageInfo.imageView = m_albedo.view;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // This descriptor is what connects set=0,binding=0 in mesh.frag to my
    // uploaded image view and sampler.
    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = m_descriptorSet;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(ctx.device(), 1, &write, 0, nullptr);
    spdlog::info("Material: descriptor set ready for '{}'", texture.path);
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

    GpuBuffer::destroyImage(ctx, m_albedo);
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
