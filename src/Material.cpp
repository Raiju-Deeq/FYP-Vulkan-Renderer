/**
 * @file Material.cpp
 * @brief Stub implementation of Material — PBR texture and descriptor management.
 *
 * ## Implementation plan (Week 4 — Milestone 3)
 *
 * ### init()
 *
 * **Step 1 — Default 1×1 albedo texture:**
 * ```cpp
 * // Create the VkImage via VMA
 * VkImageCreateInfo imgInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
 * imgInfo.imageType   = VK_IMAGE_TYPE_2D;
 * imgInfo.format      = VK_FORMAT_R8G8B8A8_SRGB;
 * imgInfo.extent      = { 1, 1, 1 };
 * imgInfo.mipLevels   = 1;
 * imgInfo.arrayLayers = 1;
 * imgInfo.samples     = VK_SAMPLE_COUNT_1_BIT;
 * imgInfo.usage       = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
 * // Prefer GpuUploader::uploadTexture2D which handles image creation, layout
 * // transitions, staging copy, and view creation in one call.
 * // Store the returned ImageResource and extract image/allocation/view from it.
 * ```
 *
 * **Step 2 — Upload white pixel via staging:**
 * ```cpp
 * uint32_t whitePixel = 0xFFFFFFFF;
 * // copy whitePixel into staging buffer then vkCmdCopyBufferToImage
 * ```
 *
 * **Step 3 — Image layout transition:**
 * The image starts in UNDEFINED layout.  Before the shader can sample from it,
 * transition it to `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL` via a barrier.
 *
 * **Step 4 — Image view:**
 * ```cpp
 * VkImageViewCreateInfo viewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
 * viewInfo.image    = m_albedoImage;
 * viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
 * viewInfo.format   = VK_FORMAT_R8G8B8A8_SRGB;
 * viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
 * vkCreateImageView(device, &viewInfo, nullptr, &m_albedoView);
 * ```
 *
 * **Step 5 — Sampler:**
 * ```cpp
 * VkSamplerCreateInfo sampInfo{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
 * sampInfo.magFilter    = VK_FILTER_LINEAR;
 * sampInfo.minFilter    = VK_FILTER_LINEAR;
 * sampInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
 * // ... etc.
 * vkCreateSampler(device, &sampInfo, nullptr, &m_sampler);
 * ```
 *
 * **Step 6 — UBO buffer (host-visible, persistently mapped):**
 * Use `GpuUploader::uploadBuffer` with `VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT` and
 * `VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT` so VMA picks a
 * host-visible heap.  For a UBO this small, persistent mapping via vmaMapMemory
 * is fine — update it every frame when constants() is mutated.
 *
 * **Step 7 — Allocate and write descriptor set:**
 * ```cpp
 * vkAllocateDescriptorSets(device, &allocInfo, &m_descriptorSet);
 * // Write UBO at binding 0:
 * VkDescriptorBufferInfo bufInfo{ m_uboBuffer, 0, sizeof(MaterialUBO) };
 * // Write sampler at binding 1:
 * VkDescriptorImageInfo  imgSampInfo{ m_sampler, m_albedoView,
 *                                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
 * // vkUpdateDescriptorSets with both writes
 * ```
 *
 * ### bindDescriptorSet()
 * ```cpp
 * vkCmdBindDescriptorSets(cmd,
 *     VK_PIPELINE_BIND_POINT_GRAPHICS,
 *     layout,
 *     1,               // set index = 1
 *     1, &m_descriptorSet,
 *     0, nullptr);     // no dynamic offsets
 * ```
 *
 * ### destroy()
 * ```cpp
 * vkDestroySampler(device, m_sampler, nullptr);
 * vkDestroyImageView(device, m_albedoView, nullptr);
 * vmaDestroyImage(allocator, m_albedoImage, m_albedoAlloc);
 * vmaDestroyBuffer(allocator, m_uboBuffer, m_uboAlloc);
 * // Descriptor set is freed implicitly when the pool is destroyed.
 * ```
 *
 * @author Mohamed Deeq Mohamed (P2884884)
 * @date   2026-03-27
 */

#include "Material.h"
#include "VulkanContext.h"

bool Material::init(const VulkanContext&  /*ctx*/,
                    VkDescriptorPool      /*pool*/,
                    VkDescriptorSetLayout /*layout*/)
{
    // TODO: Week 4 — create default texture, image view, sampler, UBO, descriptor set
    return false;
}

void Material::destroy(const VulkanContext& /*ctx*/)
{
    // TODO: Week 4 — destroy sampler, image view, image (VMA), UBO (VMA)
}

void Material::bindDescriptorSet(VkCommandBuffer  /*cmd*/,
                                  VkPipelineLayout /*layout*/) const
{
    // TODO: Week 4 — vkCmdBindDescriptorSets(cmd, GRAPHICS, layout, 1, 1, &m_descriptorSet, 0, nullptr)
}
