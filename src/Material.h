/**
 * @file Material.h
 * @brief PBR material parameters, GPU textures and descriptor set management.
 *
 * ## What a descriptor set is
 * A descriptor set is how you hand resources (textures, uniform buffers) to
 * shaders in Vulkan.  Think of it as a table of pointers:
 *
 *  - Each *binding* slot in the table holds one resource (a UBO, an image
 *    sampler, a storage buffer, etc.).
 *  - You declare the slot types upfront in a `VkDescriptorSetLayout`.
 *  - You then allocate a `VkDescriptorSet` from a `VkDescriptorPool` and
 *    fill the slots with actual `vkWriteDescriptorSet` calls.
 *  - At draw time you call `vkCmdBindDescriptorSets` to connect the set
 *    to the pipeline.
 *
 * ## PBR — Physically Based Rendering
 * PBR models how light interacts with surfaces using physically meaningful
 * parameters:
 *  - **Base colour (albedo)**: the surface colour without lighting.
 *  - **Metallic**: 0 = dielectric (plastic/stone), 1 = conductor (gold/copper).
 *  - **Roughness**: 0 = mirror, 1 = fully rough diffuse surface.
 *
 * The scalar parameters live in `MaterialUBO` (a uniform buffer object sent
 * to the fragment shader).  Texture maps override these values per-pixel.
 *
 * ## Descriptor set layout in this renderer
 * | Set | Binding | Type             | Content                |
 * |-----|---------|------------------|------------------------|
 * | 0   | 0       | Uniform buffer   | MVP matrices (M2)      |
 * | 1   | 0       | Uniform buffer   | MaterialUBO constants  |
 * | 1   | 1       | Combined sampler | Albedo texture         |
 *
 * @note  Implementation is deferred to Milestone 3 (Week 4).
 *
 * @author Mohamed Deeq Mohamed (P2884884)
 * @date   2026-03-27
 */

#ifndef FYP_VULKAN_RENDERER_MATERIAL_H
#define FYP_VULKAN_RENDERER_MATERIAL_H

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

class VulkanContext;

/**
 * @struct MaterialUBO
 * @brief Uniform buffer data block sent to the PBR fragment shader.
 *
 * This struct is uploaded verbatim to a `VkBuffer` that the fragment shader
 * reads as a uniform buffer object at `layout(set=1, binding=0)`.
 *
 * **std140 layout rules:**
 * The GLSL `std140` layout packs struct members with specific alignment
 * rules.  The `_pad` field keeps the struct size a multiple of 16 bytes,
 * matching what the GPU expects.
 *
 * The values here are multipliers applied in the shader on top of whatever
 * texture values are sampled — a factor of 1.0 means "use the texture as-is".
 */
struct MaterialUBO
{
    glm::vec4 baseColourFactor = glm::vec4(1.0f); ///< Linear-space RGBA tint (multiplied with albedo texture).
    float     metallicFactor   = 1.0f;             ///< [0,1] metallic multiplier applied in the BRDF.
    float     roughnessFactor  = 1.0f;             ///< [0,1] roughness multiplier applied in the BRDF.
    float     emissiveStrength = 1.0f;             ///< Scale factor for emissive contribution.
    float     _pad             = 0.0f;             ///< Padding to 16-byte alignment (std140 requirement).
};

/**
 * @class Material
 * @brief Owns GPU textures, a sampler, a UBO and a descriptor set for PBR shading.
 *
 * ## Default textures
 * If no external texture is loaded, Material creates a 1×1 white RGBA8 image
 * and uses it as the albedo.  Combined with `baseColourFactor`, this lets
 * colour be driven entirely by the CPU-side material constants for simple
 * objects.
 *
 * ## Ownership model
 * - **Owns:** `VkBuffer` (UBO), `VkImage` (albedo), `VkImageView`,
 *   `VkSampler`, `VkDescriptorSet`.
 * - **Does NOT own:** `VkDescriptorPool` — the pool is created by Renderer
 *   and passed in at init().
 *
 * ## Usage (M3+)
 * @code
 *   Material mat;
 *   mat.init(ctx, descriptorPool, descriptorSetLayout);
 *   mat.constants().baseColourFactor = glm::vec4(1, 0, 0, 1); // red tint
 *   // per-frame inside command recording:
 *   mat.bindDescriptorSet(cmd, pipeline.layout());
 *   mesh.draw(cmd);
 *   // teardown:
 *   mat.destroy(ctx);
 * @endcode
 */
class Material
{
public:
    // =========================================================================
    // Lifecycle
    // =========================================================================

    /**
     * @brief Creates default GPU textures and allocates the descriptor set.
     *
     * Steps (Week 4 implementation):
     *  1. Create a 1×1 RGBA8 white VkImage via VMA.
     *  2. Transition the image to `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL`
     *     using a one-time command buffer.
     *  3. Create a `VkImageView` for the albedo image.
     *  4. Create a `VkSampler` (linear filtering, repeat wrap).
     *  5. Allocate a UBO buffer via VMA for the `MaterialUBO` struct.
     *  6. Allocate a `VkDescriptorSet` from `pool` using `layout`.
     *  7. Write the UBO and sampler into the descriptor set slots.
     *
     * @param  ctx     Initialised VulkanContext.
     * @param  pool    Descriptor pool from which to allocate the set.
     *                 Created by Renderer to cover the maximum descriptor count.
     * @param  layout  Descriptor set layout matching set 1 in the shaders.
     * @return true    on success.
     *
     * @note  Implementation deferred to Week 4 (Milestone 3).
     */
    bool init(const VulkanContext&  ctx,
              VkDescriptorPool      pool,
              VkDescriptorSetLayout layout);

    /**
     * @brief Destroys all owned GPU resources.
     *
     * Destroys sampler, image view, albedo image (+ VMA allocation), and
     * the UBO buffer (+ VMA allocation).  The descriptor set does not need
     * to be explicitly freed if the pool is destroyed at teardown.
     *
     * @param ctx  The same VulkanContext passed to init().
     *
     * @note  Implementation deferred to Week 4 (Milestone 3).
     */
    void destroy(const VulkanContext& ctx);

    // =========================================================================
    // Render-time
    // =========================================================================

    /**
     * @brief Binds this material's descriptor set for the next draw call.
     *
     * Records `vkCmdBindDescriptorSets` targeting set index 1.
     * Set 0 (MVP matrices) is bound separately by the Renderer.
     *
     * @param cmd     Active command buffer in recording state.
     * @param layout  The pipeline layout (must match the one used in init()).
     *
     * @note  Implementation deferred to Week 4 (Milestone 3).
     */
    void bindDescriptorSet(VkCommandBuffer  cmd,
                           VkPipelineLayout layout) const;

    // =========================================================================
    // Accessors / mutators
    // =========================================================================

    /// @brief Mutable reference to CPU-side material constants.
    /// Modify these then upload via a UBO update (mapped memory or staging)
    /// to change the material appearance at runtime.
    MaterialUBO&       constants()       { return m_constants; }

    /// @brief Const reference to the CPU-side material constants.
    const MaterialUBO& constants() const { return m_constants; }

private:
    MaterialUBO     m_constants;                      ///< CPU mirror of the GPU UBO.
    VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE; ///< Descriptor set bound at draw time.
    VkBuffer        m_uboBuffer     = VK_NULL_HANDLE; ///< GPU-side UBO (device-local or host-visible).
    VkImage         m_albedoImage   = VK_NULL_HANDLE; ///< Albedo texture (default: 1×1 white).
    VkImageView     m_albedoView    = VK_NULL_HANDLE; ///< View over the albedo image.
    VkSampler       m_sampler       = VK_NULL_HANDLE; ///< Sampler: linear filtering, repeat wrap.
};

#endif // FYP_VULKAN_RENDERER_MATERIAL_H
