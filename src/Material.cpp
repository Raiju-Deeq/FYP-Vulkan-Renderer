/**
 * @file Material.cpp
 * @brief Implementation of Material - PBR texture and descriptor management.
 *
 * @author Mohamed Deeq Mohamed
 * @date   2026-03-27
 *
 * @todo  Week 4 - implement init(): create 1x1 default textures via
 *                 vkCreateImage + VMA, build image views, create sampler,
 *                 allocate + write descriptor set.
 * @todo  Week 4 - implement destroy(): destroy sampler, image views,
 *                 images and UBO buffer via VMA.
 * @todo  Week 4 - implement bindDescriptorSet(): vkCmdBindDescriptorSets
 *                 at set index 1.
 */

#include "Material.h"
#include "VulkanContext.h"

bool Material::init(const VulkanContext&  /*ctx*/,
                    VkDescriptorPool      /*pool*/,
                    VkDescriptorSetLayout /*layout*/)
{
    // TODO: Week 4 - default texture + descriptor set creation
    return false;
}

void Material::destroy(const VulkanContext& /*ctx*/)
{
    // TODO: Week 4 - destroy all GPU resources
}

void Material::bindDescriptorSet(VkCommandBuffer  /*cmd*/,
                                  VkPipelineLayout /*layout*/) const
{
    // TODO: Week 4 - vkCmdBindDescriptorSets (set = 1)
}
