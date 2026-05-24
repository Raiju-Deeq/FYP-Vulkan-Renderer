/**
 * @file gaussian_splat.hpp
 * @brief Minimal Gaussian-style point splat loader and renderer.
 *
 * This module implements the C3 stretch objective in a controlled way.  It
 * ingests an ASCII or binary-little-endian `.ply` point cloud, repacks each
 * point into a compact GPU record, uploads those records into a VMA-backed
 * storage buffer, and renders each point as a soft alpha-blended billboard.
 *
 * This is intentionally not a full research-grade 3D Gaussian Splatting
 * implementation yet.  Full 3DGS needs covariance projection, spherical
 * harmonics, and depth-sorted alpha compositing.  Here I build the smallest
 * version that proves the renderer can load splat data and draw it on screen.
 */

#ifndef FYP_VULKAN_RENDERER_GAUSSIAN_SPLAT_HPP
#define FYP_VULKAN_RENDERER_GAUSSIAN_SPLAT_HPP

#include "gpu_buffer.hpp"

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#include <cstdint>
#include <string>
#include <vector>

class VulkanContext;

/**
 * @struct GaussianPoint
 * @brief CPU/GPU record for one rendered splat.
 *
 * The layout is deliberately two `vec4`s so std430 storage-buffer layout in
 * GLSL matches the C++ memory layout cleanly.
 */
struct GaussianPoint
{
    glm::vec4 positionOpacity{0.0f, 0.0f, 0.0f, 1.0f}; ///< XYZ position, W opacity.
    glm::vec4 colorRadius{1.0f, 1.0f, 1.0f, 0.025f};  ///< RGB colour, W base radius.
};

/**
 * @class GaussianSplat
 * @brief Owns a `.ply` splat point buffer and the alpha-blended splat pipeline.
 *
 * The class keeps the stretch feature separate from the mesh/PBR renderer so
 * I can demonstrate splats without disturbing the stable OBJ path.
 */
class GaussianSplat
{
public:
    /**
     * @brief Builds the descriptor layout, pipeline layout and graphics pipeline.
     *
     * @param ctx Initialised Vulkan context.
     * @param vertSpvPath Compiled splat vertex shader path.
     * @param fragSpvPath Compiled splat fragment shader path.
     * @param colourFormat Swapchain colour format used by Dynamic Rendering.
     * @param depthFormat Depth attachment format used by Dynamic Rendering.
     * @return true if all Vulkan pipeline objects were created.
     */
    bool initPipeline(const VulkanContext& ctx,
                      const std::string& vertSpvPath,
                      const std::string& fragSpvPath,
                      VkFormat colourFormat,
                      VkFormat depthFormat);

    /**
     * @brief Destroys only the pipeline objects.
     *
     * I call this during swapchain rebuild because attachment formats are tied
     * to the pipeline, while the uploaded point buffer can remain alive.
     *
     * @param ctx Context that owns the VkDevice.
     */
    void destroyPipeline(const VulkanContext& ctx);

    /**
     * @brief Loads a `.ply` point cloud and uploads it to a storage buffer.
     *
     * Required properties are `x`, `y` and `z`.  Optional properties are
     * `red`/`green`/`blue`, `opacity`, and `scale_0`/`scale_1`/`scale_2`.
     * Missing optional values receive simple defaults so ordinary point clouds
     * can still be displayed.
     *
     * @param ctx Initialised Vulkan context.
     * @param path Filesystem path to the `.ply` file.
     * @return true if at least one point was loaded and uploaded.
     */
    bool loadFromPly(const VulkanContext& ctx, const std::string& path);

    /**
     * @brief Records the splat draw call into the active command buffer.
     *
     * @param cmd Command buffer currently inside `vkCmdBeginRendering`.
     * @param mvp Model-view-projection matrix used to place splats in the scene.
     * @param radiusScale Runtime multiplier for splat size.
     * @param opacityScale Runtime multiplier for alpha.
     */
    void draw(VkCommandBuffer cmd,
              const glm::mat4& mvp,
              float radiusScale,
              float opacityScale) const;

    /**
     * @brief Destroys pipeline, descriptor and buffer resources.
     * @param ctx Context that owns the VkDevice and VMA allocator.
     */
    void destroy(const VulkanContext& ctx);

    /**
     * @brief Returns true when both pipeline and point buffer are ready to draw.
     * @return true if draw() can be called safely.
     */
    bool isDrawable() const;

    /**
     * @brief Number of points currently uploaded.
     * @return Uploaded splat count.
     */
    uint32_t pointCount() const { return m_pointCount; }

private:
    /**
     * @brief Reads a SPIR-V shader file.
     * @param path Path to `.spv` bytecode.
     * @return SPIR-V words, or empty vector on failure.
     */
    static std::vector<uint32_t> loadSpv(const std::string& path);

    /**
     * @brief Creates a temporary shader module for pipeline compilation.
     * @param device Logical device.
     * @param code SPIR-V bytecode.
     * @return Shader module handle, or `VK_NULL_HANDLE` on failure.
     */
    static VkShaderModule createShaderModule(VkDevice device,
                                             const std::vector<uint32_t>& code);

    GpuBuffer::BufferResource m_pointBuffer{}; ///< Device-local storage buffer of GaussianPoint records.
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    uint32_t m_pointCount = 0;
};

#endif // FYP_VULKAN_RENDERER_GAUSSIAN_SPLAT_HPP
