/**
 * @file graphics_pipeline.hpp
 * @brief SPIR-V shader loading and Dynamic Rendering graphics pipeline.
 *
 * ## What a graphics pipeline is
 * A `VkPipeline` is a compiled, immutable state object that encodes *all*
 * the fixed-function and programmable state the GPU will use for a draw call:
 *  - Which vertex and fragment shaders to run (programmable stages).
 *  - How to assemble vertex data (topology, vertex input layout).
 *  - Rasteriser settings (fill mode, face culling, winding order).
 *  - Colour blending (how the output pixel merges with existing content).
 *  - Which states are *dynamic* (overridden per-frame instead of baked in).
 *
 * Unlike OpenGL, Vulkan compiles all of this into a single GPU-native object
 * upfront.  This is expensive at creation time but eliminates hidden
 * recompilations mid-frame.
 *
 * ## Dynamic Rendering
 * This pipeline uses **Vulkan 1.3 Dynamic Rendering** — there is no
 * `VkRenderPass` or `VkFramebuffer` anywhere in this codebase.  Instead, the
 * pipeline declares the colour attachment *format* at creation time via
 * `VkPipelineRenderingCreateInfo` (chained into `pNext`), and the actual
 * image view is bound per-frame via `vkCmdBeginRendering`.
 *
 * ## M1 pipeline specifics
 *  - No vertex input (positions hardcoded in `triangle.vert`).
 *  - No descriptor sets (no UBOs, no textures until M2+).
 *  - Dynamic viewport + scissor so the pipeline survives window resize.
 *  - Back-face culling disabled (the test triangle has no closed surface).
 *
 * @author Mohamed Deeq Mohamed (P2884884)
 * @date   2026-03-27
 */

#ifndef FYP_VULKAN_RENDERER_PIPELINE_HPP
#define FYP_VULKAN_RENDERER_PIPELINE_HPP

#include <vulkan/vulkan.h>
#include <string>
#include <vector>

class VulkanContext;

/**
 * @class Pipeline
 * @brief I own a compiled Vulkan graphics pipeline and its associated layout.
 *
 * ## Ownership model
 * - **I own:** `VkPipeline`, `VkPipelineLayout`, optionally `VkDescriptorSetLayout`.
 * - **I do NOT own:** shader modules — I create them temporarily during
 *   init() and destroy them immediately after pipeline compilation.
 *
 * ## Dependency on SwapChain
 * I require the swapchain colour format so I can declare the correct attachment
 * format for Dynamic Rendering.  If the swapchain is rebuilt with a different
 * format (rare but possible), I must also be rebuilt — see the resize path in main.cpp.
 *
 * ## Usage
 * @code
 *   Pipeline pipeline;
 *   pipeline.init(ctx, "shaders/triangle.vert.spv",
 *                      "shaders/triangle.frag.spv",
 *                      swap.format());
 *   // per-frame:
 *   vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.handle());
 *   // teardown:
 *   pipeline.destroy(ctx);
 * @endcode
 */
class Pipeline
{
public:
    // =========================================================================
    // Lifecycle
    // =========================================================================

    /**
     * @brief Loads shaders and compiles the complete graphics pipeline.
     *
     * **Build sequence:**
     *  1. Read `.spv` bytecode for both shaders (loadSpv).
     *  2. Wrap bytecode in temporary `VkShaderModule` objects.
     *  3. Declare shader stage descriptors (vertex + fragment, entry = "main").
     *  4. Configure fixed-function state: vertex input, input assembly,
     *     rasteriser, multisampling, colour blend, dynamic state.
     *  5. Create an empty `VkPipelineLayout` (no descriptors for M1).
     *  6. Chain `VkPipelineRenderingCreateInfo` into `pNext` to declare
     *     the colour attachment format — this replaces `VkRenderPass`.
     *  7. Call `vkCreateGraphicsPipelines`.
     *  8. Destroy the shader modules (driver keeps its own compiled copy).
     *
     * @param  ctx           Initialised VulkanContext.
     * @param  vertSpvPath   Filesystem path to the compiled vertex shader `.spv`.
     * @param  fragSpvPath   Filesystem path to the compiled fragment shader `.spv`.
     * @param  colourFormat  Swapchain image format — must match the attachment
     *                       format used in `vkCmdBeginRendering`.
     * @return true          on success.
     * @return false         if shaders cannot be read or any Vulkan call fails.
     */
    bool init(const VulkanContext& ctx,
              const std::string&   vertSpvPath,
              const std::string&   fragSpvPath,
              VkFormat             colourFormat);

    /**
     * @brief Destroys the pipeline, layout and descriptor set layout.
     *
     * Each handle is guarded against `VK_NULL_HANDLE`, so this is safe to
     * call after a partial init() failure or on a default-constructed Pipeline.
     *
     * @param ctx  The same VulkanContext passed to init().
     */
    void destroy(const VulkanContext& ctx);

    // =========================================================================
    // Accessors
    // =========================================================================

    /// @brief The compiled graphics pipeline.
    /// Passed to `vkCmdBindPipeline` at the start of each frame's draw commands.
    VkPipeline            handle()             const { return m_pipeline; }

    /// @brief The pipeline layout.
    /// Required when calling `vkCmdBindDescriptorSets` or `vkCmdPushConstants`
    /// (used from M2 onwards when UBOs and materials are introduced).
    VkPipelineLayout      layout()             const { return m_layout; }

    /// @brief Descriptor set layout for set 0 — null until M2 adds the MVP UBO.
    /// A set layout describes the binding slots (UBOs, combined image samplers,
    /// etc.) that the shaders in this pipeline can access.
    VkDescriptorSetLayout descriptorSetLayout() const { return m_descriptorSetLayout; }

private:
    // =========================================================================
    // Private helpers
    // =========================================================================

    /**
     * @brief Reads a SPIR-V `.spv` file into a `uint32_t` vector.
     *
     * Opens in binary mode, seeks to end to measure size, then reads the
     * entire file.  SPIR-V is guaranteed to be 4-byte aligned by the spec —
     * the byte count is validated before returning.
     *
     * @param  path  Filesystem path to the `.spv` file.
     * @return Non-empty vector on success; empty vector if the file cannot be
     *         opened, is empty, or has a size that isn't a multiple of 4.
     */
    static std::vector<uint32_t> loadSpv(const std::string& path);

    /**
     * @brief Wraps raw SPIR-V bytecode in a `VkShaderModule`.
     *
     * A shader module is a thin handle that holds the bytecode until the
     * pipeline is compiled.  After `vkCreateGraphicsPipelines` the module
     * is no longer needed and should be destroyed to free driver memory.
     *
     * @param  device  Logical device handle.
     * @param  code    SPIR-V bytecode returned by loadSpv().
     * @return Valid `VkShaderModule` on success; `VK_NULL_HANDLE` on failure.
     */
    static VkShaderModule createShaderModule(VkDevice                     device,
                                             const std::vector<uint32_t>& code);

    // =========================================================================
    // Owned handles
    // =========================================================================

    /// Compiled graphics pipeline — immutable once created; can only be destroyed.
    VkPipeline            m_pipeline            = VK_NULL_HANDLE;

    /// Describes the interface between the pipeline and its descriptor sets /
    /// push constants.  Empty for M1 (no descriptors).
    VkPipelineLayout      m_layout              = VK_NULL_HANDLE;

    /// Descriptor set layout for set 0 — null for M1, populated in M2.
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
};

#endif // FYP_VULKAN_RENDERER_PIPELINE_HPP
