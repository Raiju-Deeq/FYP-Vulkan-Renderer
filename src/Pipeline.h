/**
 * @file Pipeline.h
 * @brief Vulkan graphics pipeline builder and shader module management.
 *
 * Compiles SPIR-V shaders loaded from disk and assembles the full
 * VkPipeline object using Dynamic Rendering (no render pass object
 * required in Vulkan 1.3).  PipelineLayout and descriptor set layouts
 * are also owned here.
 *
 * @author Mohamed Deeq Mohamed
 * @date   2026-03-27
 */

#ifndef FYP_VULKAN_RENDERER_PIPELINE_H
#define FYP_VULKAN_RENDERER_PIPELINE_H

#include <vulkan/vulkan.h>
#include <string>
#include <vector>

class VulkanContext;

/**
 * @class Pipeline
 * @brief Owns a compiled Vulkan graphics pipeline and its layout.
 *
 * The pipeline is configured for Dynamic Rendering (Vulkan 1.3
 * VK_KHR_dynamic_rendering), so no VkRenderPass is needed.  The colour
 * attachment format must match the swapchain format.
 *
 * @note  Shader SPIR-V paths are relative to the working directory.
 *        Use the CMake custom command that compiles .vert/.frag to .spv.
 */
class Pipeline
{
public:
    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------

    /**
     * @brief Loads SPIR-V shaders and builds the graphics pipeline.
     *
     * @param  ctx              Initialised VulkanContext.
     * @param  vertSpvPath      Path to the compiled vertex shader (.spv).
     * @param  fragSpvPath      Path to the compiled fragment shader (.spv).
     * @param  colourFormat     VkFormat matching the target colour attachment
     *                          (typically the swapchain image format).
     * @return true             on success.
     * @return false            if shader loading or pipeline creation fails.
     */
    bool init(const VulkanContext& ctx,
              const std::string&   vertSpvPath,
              const std::string&   fragSpvPath,
              VkFormat             colourFormat);

    /**
     * @brief Destroys the pipeline, pipeline layout and descriptor set layout.
     * @param ctx  The same VulkanContext passed to init().
     */
    void destroy(const VulkanContext& ctx);

    // -------------------------------------------------------------------------
    // Accessors
    // -------------------------------------------------------------------------

    /// @brief Returns the compiled graphics pipeline handle.
    VkPipeline            handle()              const { return m_pipeline; }

    /// @brief Returns the pipeline layout handle.
    VkPipelineLayout      layout()              const { return m_layout; }

    /// @brief Returns the descriptor set layout (uniform buffers, samplers).
    VkDescriptorSetLayout descriptorSetLayout() const { return m_descriptorSetLayout; }

private:
    /**
     * @brief Reads a binary SPIR-V file and returns its contents.
     * @param  path  Filesystem path to the .spv file.
     * @return Byte vector containing SPIR-V bytecode, or empty on failure.
     */
    static std::vector<uint32_t> loadSpv(const std::string& path);

    /**
     * @brief Wraps SPIR-V bytecode in a VkShaderModule.
     * @param  device  Logical device.
     * @param  code    SPIR-V bytecode (must be 4-byte aligned).
     * @return Valid VkShaderModule or VK_NULL_HANDLE on failure.
     */
    static VkShaderModule createShaderModule(VkDevice device,
                                             const std::vector<uint32_t>& code);

    VkPipeline            m_pipeline            = VK_NULL_HANDLE; ///< Compiled pipeline.
    VkPipelineLayout      m_layout              = VK_NULL_HANDLE; ///< Pipeline layout.
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE; ///< Descriptor layout.
};

#endif // FYP_VULKAN_RENDERER_PIPELINE_H
