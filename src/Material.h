/**
 * @file Material.h
 * @brief One sampled albedo texture and descriptor set for the M2 mesh path.
 */

#ifndef FYP_VULKAN_RENDERER_MATERIAL_H
#define FYP_VULKAN_RENDERER_MATERIAL_H

#include "AssetLoader/GpuUploader.h"

#include <vulkan/vulkan.h>

class VulkanContext;

class Material
{
public:
    bool init(const VulkanContext&             ctx,
              const AssetLoader::LoadedTexture& texture,
              VkDescriptorSetLayout           layout);

    void destroy(const VulkanContext& ctx);

    void bindDescriptorSet(VkCommandBuffer  cmd,
                           VkPipelineLayout layout) const;

private:
    AssetLoader::ImageResource m_albedo{};
    VkSampler                  m_sampler = VK_NULL_HANDLE;
    VkDescriptorPool           m_descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet            m_descriptorSet = VK_NULL_HANDLE;
};

#endif // FYP_VULKAN_RENDERER_MATERIAL_H
