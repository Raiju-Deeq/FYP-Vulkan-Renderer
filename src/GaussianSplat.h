/**
 * @file GaussianSplat.h
 * @brief 3D Gaussian Splatting data loader and GPU render preparation.
 *
 * Implements a basic forward-rendered approximation of 3D Gaussian
 * Splatting (Kerbl et al., SIGGRAPH 2023).  Loads a pre-trained .ply
 * splat file, uploads Gaussian attributes (position, covariance, colour,
 * opacity) to a GPU storage buffer, and exposes a draw() method that
 * issues an indirect instanced draw call — one quad instance per Gaussian.
 *
 * @note  This is the stretch-goal feature (M6 Could-Have).  The class is
 *        stubbed in Week 1 so the architecture is visible from the outset;
 *        implementation begins only after M4 (PBR) is solid.
 *
 * @author Mohamed Deeq Mohamed
 * @date   2026-03-28
 *
 * @see   Kerbl, B. et al. (2023) "3D Gaussian Splatting for Real-Time
 *        Radiance Field Rendering", ACM Transactions on Graphics 42(4).
 */

#ifndef VULKAN_RENDERER_GAUSSIANSPLAT_H
#define VULKAN_RENDERER_GAUSSIANSPLAT_H

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>

class VulkanContext;

/**
 * @struct GaussianPoint
 * @brief CPU-side representation of a single 3D Gaussian primitive.
 *
 * Each field maps directly to a .ply property produced by the official
 * 3DGS training code.  The struct is packed to match the GLSL storage
 * buffer layout declared in the Gaussian splat shaders.
 */
struct GaussianPoint
{
    glm::vec3 position;    ///< World-space centre of the Gaussian (x,y,z).
    float     opacity;     ///< Pre-sigmoid opacity; apply sigma() in shader.
    glm::vec3 scale;       ///< Log-scale values (x,y,z); apply exp() in shader.
    glm::vec4 rotation;    ///< Unit quaternion (w,x,y,z) encoding orientation.
    glm::vec3 shCoeffsDC;  ///< Degree-0 spherical harmonic (base colour, RGB).
};

/**
 * @class GaussianSplat
 * @brief Loads and renders a 3DGS point cloud via GPU-accelerated splatting.
 *
 * Usage sketch (implementation deferred to M6):
 * @code
 *   GaussianSplat gs;
 *   gs.loadPly("scene.ply");
 *   gs.uploadToGPU(ctx);
 *   // inside render loop:
 *   gs.sortByDepth(viewMatrix);
 *   gs.draw(cmd);
 * @endcode
 */
class GaussianSplat
{
public:
    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------

    /**
     * @brief Loads Gaussian attributes from a .ply file into CPU memory.
     *
     * Parses position, opacity, scale, rotation and degree-0 SH colour
     * coefficients.  Higher-degree SH bands are currently ignored.
     *
     * @param  plyPath  Filesystem path to the trained .ply splat file.
     * @return true     on success.
     * @return false    if the file cannot be opened or does not contain
     *                  the expected property layout.
     */
    bool loadPly(const std::string& plyPath);

    /**
     * @brief Uploads the CPU Gaussian array to a device-local GPU storage buffer.
     * @param  ctx  Initialised VulkanContext.
     * @return true on success.
     */
    bool uploadToGPU(const VulkanContext& ctx);

    /**
     * @brief Destroys the GPU storage buffer and any associated resources.
     * @param ctx  The same VulkanContext passed to uploadToGPU().
     */
    void destroy(const VulkanContext& ctx);

    // -------------------------------------------------------------------------
    // Render-time
    // -------------------------------------------------------------------------

    /**
     * @brief Sorts Gaussians back-to-front relative to the camera.
     *
     * A depth sort is required for correct alpha-composited splatting.
     * This is a CPU radix sort over the Gaussian depth values;
     * a GPU compute sort is the stretch-goal optimisation.
     *
     * @param viewMatrix  Current camera view matrix (world to camera space).
     */
    void sortByDepth(const glm::mat4& viewMatrix);

    /**
     * @brief Records the instanced splat draw call into the command buffer.
     *
     * Binds the Gaussian storage buffer as a vertex/storage resource and
     * issues vkCmdDraw for (splatCount x 4) vertices (two triangles per
     * splat as a screen-aligned quad expanded in the vertex shader).
     *
     * @param cmd  Active command buffer in recording state.
     */
    void draw(VkCommandBuffer cmd) const;

    // -------------------------------------------------------------------------
    // Accessors
    // -------------------------------------------------------------------------

    /// @brief Returns the number of Gaussian primitives loaded.
    uint32_t splatCount() const
        { return static_cast<uint32_t>(m_gaussians.size()); }

private:
    std::vector<GaussianPoint> m_gaussians;                  ///< CPU-side splat data.
    VkBuffer                   m_gpuBuffer = VK_NULL_HANDLE; ///< Device-local storage buffer.
};

#endif // VULKAN_RENDERER_GAUSSIANSPLAT_H
