/**
 * @file Material.h
 * @brief PBR material parameters and associated descriptor set management.
 *
 * A Material bundles the PBR textures (albedo, normal, metallic-roughness,
 * AO, emissive) with a VkDescriptorSet that binds them for use in the
 * fragment shader.  It also stores the UBO with scalar material constants.
 *
 * @author Mohamed Deeq Mohamed
 * @date   2026-03-27
 */

#ifndef FYP_VULKAN_RENDERER_MATERIAL_H
#define FYP_VULKAN_RENDERER_MATERIAL_H

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

class VulkanContext;

/**
 * @struct MaterialUBO
 * @brief Uniform buffer object mirroring the material constants in the
 *        fragment shader's UBO block.
 *
 * Matches the GLSL layout:
 * @code
 *   layout(set=1, binding=0) uniform MaterialUBO { ... };
 * @endcode
 */
struct MaterialUBO
{
    glm::vec4 baseColourFactor = glm::vec4(1.0f); ///< Linear RGBA tint applied to albedo texture.
    float     metallicFactor   = 1.0f;             ///< [0,1] metallic multiplier.
    float     roughnessFactor  = 1.0f;             ///< [0,1] roughness multiplier.
    float     emissiveStrength = 1.0f;             ///< Emissive intensity scale.
    float     _pad             = 0.0f;             ///< std140 padding to 16-byte boundary.
};

/**
 * @class Material
 * @brief PBR material owning GPU textures, sampler and descriptor set.
 *
 * After init() the material is ready to be bound with bindDescriptorSet()
 * before a draw call.  Textures are lazily created: if no texture path
 * is provided a 1x1 default texture is used.
 */
class Material
{
public:
    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------

    /**
     * @brief Allocates default (1x1 white) textures and creates the
     *        descriptor set referencing them.
     *
     * @param  ctx    Initialised VulkanContext.
     * @param  pool   Descriptor pool from which to allocate the set.
     * @param  layout Descriptor set layout matching the pipeline's set 1.
     * @return true   on success.
     */
    bool init(const VulkanContext&  ctx,
              VkDescriptorPool      pool,
              VkDescriptorSetLayout layout);

    /**
     * @brief Destroys owned textures, image views, sampler and UBO buffer.
     * @param ctx  The same VulkanContext passed to init().
     */
    void destroy(const VulkanContext& ctx);

    // -------------------------------------------------------------------------
    // Render-time
    // -------------------------------------------------------------------------

    /**
     * @brief Binds this material's descriptor set into the command buffer.
     *
     * @param cmd     Active command buffer in recording state.
     * @param layout  Pipeline layout used for the bind call.
     */
    void bindDescriptorSet(VkCommandBuffer  cmd,
                           VkPipelineLayout layout) const;

    // -------------------------------------------------------------------------
    // Accessors / mutators
    // -------------------------------------------------------------------------

    /// @brief Returns a mutable reference to the CPU-side material constants.
    MaterialUBO&       constants()       { return m_constants; }

    /// @brief Returns a const reference to the CPU-side material constants.
    const MaterialUBO& constants() const { return m_constants; }

private:
    MaterialUBO     m_constants;                      ///< CPU-side constant data.
    VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE; ///< GPU descriptor set.
    VkBuffer        m_uboBuffer     = VK_NULL_HANDLE; ///< Uniform buffer for MaterialUBO.
    VkImage         m_albedoImage   = VK_NULL_HANDLE; ///< Albedo / base colour texture.
    VkImageView     m_albedoView    = VK_NULL_HANDLE; ///< Image view for albedo.
    VkSampler       m_sampler       = VK_NULL_HANDLE; ///< Shared texture sampler.
};

#endif // FYP_VULKAN_RENDERER_MATERIAL_H
