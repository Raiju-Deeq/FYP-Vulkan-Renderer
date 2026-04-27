/**
 * @file Material.cpp
 * @brief Descriptor and sampler setup for the M2 textured mesh.
 */

#include "Material.h"

#include "VulkanContext.h"

#include <spdlog/spdlog.h>

bool Material::init(const VulkanContext&              ctx,
                    const AssetLoader::LoadedTexture& texture,
                    VkDescriptorSetLayout             layout)
{
    destroy(ctx);

    if (!AssetLoader::GpuUploader::uploadTexture2D(ctx, texture, m_albedo)) {
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
    if (m_descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(ctx.device(), m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
        m_descriptorSet = VK_NULL_HANDLE;
    }

    if (m_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(ctx.device(), m_sampler, nullptr);
        m_sampler = VK_NULL_HANDLE;
    }

    AssetLoader::GpuUploader::destroyImage(ctx, m_albedo);
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
