/**
 * @file gaussian_splat.hpp
 * @brief Gaussian-style `.ply` loader and GPU-projected splat renderer.
 *
 * This module implements the C3 stretch objective in a controlled way.  It
 * ingests an ASCII or binary-little-endian `.ply` point cloud, converts the
 * Gaussian attributes into a compact GPU storage-buffer layout, and lets the
 * vertex shader project every splat into screen space each frame.
 *
 * This stays deliberately scoped as a stretch feature: it previews full PLY
 * splat clouds with GPU-side projection while keeping the stable OBJ renderer
 * independent from the splat pipeline.
 */

#ifndef FYP_VULKAN_RENDERER_GAUSSIAN_SPLAT_HPP
#define FYP_VULKAN_RENDERER_GAUSSIAN_SPLAT_HPP

#include "gpu_buffer.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

class VulkanContext;

/**
 * @struct GaussianPoint
 * @brief CPU-side source data for one loaded 3D Gaussian.
 *
 * I keep this as the parsing/intermediate form.  Once import finishes, the
 * data is packed into `GaussianGpuPoint` and uploaded to a device-local storage
 * buffer so the render loop no longer touches every splat on the CPU.
 */
struct GaussianPoint
{
    glm::vec3 position{0.0f}; ///< Normalised world-space position.
    glm::vec3 color{1.0f};    ///< Base colour recovered from RGB or SH DC values.
    float opacity = 1.0f;     ///< Activated opacity in 0..1.
    glm::vec3 scale{0.01f};   ///< Activated 3D Gaussian scale.
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f}; ///< Normalised Gaussian rotation.
    std::array<float, 6> covariance{}; ///< Precomputed upper triangle of the 3D covariance matrix.
};

/**
 * @struct GaussianGpuPoint
 * @brief Device-side Gaussian record consumed directly by the splat vertex shader.
 *
 * The layout uses four `vec4`-sized rows so std430 storage-buffer alignment is
 * simple and predictable in both C++ and GLSL.  I precompute the 3D covariance
 * once during import, then the shader only has to project it through the active
 * camera each frame.
 */
struct GaussianGpuPoint
{
    glm::vec4 positionOpacity{0.0f}; ///< xyz = normalised position, w = activated opacity.
    glm::vec4 color{1.0f}; ///< rgb = base colour, w unused padding.
    glm::vec4 covarianceA{0.0f}; ///< xx, xy, xz, yy from the symmetric 3D covariance.
    glm::vec4 covarianceB{0.0f}; ///< yz, zz, unused, unused from the symmetric 3D covariance.
};

/**
 * @class GaussianSplat
 * @brief Owns `.ply` Gaussian data, one GPU storage buffer, and the splat pipeline.
 *
 * The class keeps the stretch feature separate from the mesh/PBR renderer, so
 * I can work on splats without risking the stable OBJ path.
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
     * @brief Destroys only the graphics pipeline objects.
     *
     * I call this during swapchain rebuild because attachment formats are tied
     * to the pipeline.  The descriptor layout and uploaded buffers stay alive
     * so the loaded splat file survives a resize.
     *
     * @param ctx Context that owns the VkDevice.
     */
    void destroyPipeline(const VulkanContext& ctx);

    /**
     * @brief Loads a `.ply` point cloud and uploads it to a storage buffer.
     *
     * Required properties are `x`, `y` and `z`.  Optional properties are
     * `red`/`green`/`blue`, `f_dc_0..2`, `opacity`, `scale_0..2`, and
     * `rot_0..3`.  Missing optional values receive defaults so ordinary point
     * clouds can still be displayed.
     *
     * @param ctx Initialised Vulkan context.
     * @param path Filesystem path to the `.ply` file.
     * @return true if at least one point was loaded and uploaded.
     */
    bool loadFromPly(const VulkanContext& ctx, const std::string& path);

    /**
     * @brief Records the full GPU-projected splat draw call.
     *
     * @param cmd Command buffer currently inside `vkCmdBeginRendering`.
     * @param modelView Model-to-view matrix; I pass the same rotating model transform as OBJ.
     * @param projection Active view-to-clip projection matrix.
     * @param extent Current swapchain extent in pixels.
     * @param radiusScale Runtime multiplier for splat size.
     * @param opacityScale Runtime multiplier for alpha.
     */
    void draw(VkCommandBuffer cmd,
              const glm::mat4& modelView,
              const glm::mat4& projection,
              VkExtent2D extent,
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

    GpuBuffer::BufferResource m_pointBuffer{}; ///< Device-local source Gaussian buffer read by the vertex shader.
    VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    uint32_t m_pointCount = 0;
};

#endif // FYP_VULKAN_RENDERER_GAUSSIAN_SPLAT_HPP
