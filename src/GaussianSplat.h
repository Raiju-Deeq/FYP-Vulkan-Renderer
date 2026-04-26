/**
 * @file GaussianSplat.h
 * @brief 3D Gaussian Splatting scene loader and GPU render preparation.
 *
 * ## What 3D Gaussian Splatting is
 * 3DGS (Kerbl et al., SIGGRAPH 2023) represents a scene as a large collection
 * of semi-transparent 3D Gaussian "blobs" (ellipsoids) rather than triangle
 * meshes.  Each Gaussian has:
 *  - A **position** (centre of the ellipsoid).
 *  - A **covariance** encoded as scale + rotation (shape and orientation).
 *  - An **opacity** (how transparent the blob is).
 *  - A **colour** encoded as spherical harmonic coefficients (view-dependent).
 *
 * At render time, each Gaussian is projected onto the screen as a 2D splat
 * (typically a screen-aligned quad) and alpha-composited back-to-front.
 *
 * ## Why order matters (alpha compositing)
 * The splats are semi-transparent and must be rendered back-to-front (painter's
 * algorithm) for correct blending.  A depth sort (`sortByDepth`) is therefore
 * required every frame as the camera moves.
 *
 * ## File format
 * Trained scenes are exported as `.ply` files (Polygon File Format) in
 * `binary_little_endian` format by the official 3DGS training code
 * (github.com/graphdeco-inria/gaussian-splatting).  Each point in the file
 * carries the attributes that map to `GaussianPoint`.
 *
 * ## Implementation status
 * This is a **Could-Have stretch goal (C3)** — scaffolded in Week 1 so the
 * architecture is visible, but implementation begins only after the core
 * renderer (M4) is complete and stable.
 *
 * @see   Kerbl, B. et al. (2023) "3D Gaussian Splatting for Real-Time
 *        Radiance Field Rendering", ACM Transactions on Graphics 42(4).
 *        https://repo-sam.inria.fr/fungraph/3d-gaussian-splatting/
 *
 * @author Mohamed Deeq Mohamed (P2884884)
 * @date   2026-03-28
 */

#ifndef FYP_VULKAN_RENDERER_GAUSSIANSPLAT_H
#define FYP_VULKAN_RENDERER_GAUSSIANSPLAT_H

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>

class VulkanContext;

/**
 * @struct GaussianPoint
 * @brief CPU-side representation of a single 3D Gaussian primitive.
 *
 * Each field maps directly to a property produced by the official 3DGS
 * training pipeline.  The struct is also intended to mirror the GLSL storage
 * buffer layout declared in `splat.vert` (M6 shader).
 *
 * **Encoding conventions (applied in the vertex shader, not here):**
 *  - `opacity` is pre-sigmoid: the shader computes `1 / (1 + exp(-opacity))`.
 *  - `scale` is log-scale: the shader computes `exp(scale)` per axis.
 *  - `rotation` is a unit quaternion: the shader converts it to a 3×3 matrix.
 *  - `shCoeffsDC` are degree-0 spherical harmonic coefficients: they give the
 *    view-independent base colour component (`colour = 0.5 + 0.282 * shDC`).
 */
struct GaussianPoint
{
    glm::vec3 position;    ///< World-space centre of the Gaussian (x, y, z).
    float     opacity;     ///< Pre-sigmoid opacity — apply σ(x) = 1/(1+e^−x) in shader.
    glm::vec3 scale;       ///< Log-scale per axis (x, y, z) — apply exp() in shader.
    glm::vec4 rotation;    ///< Unit quaternion (w, x, y, z) encoding 3D orientation.
    glm::vec3 shCoeffsDC;  ///< Degree-0 SH colour (R, G, B) — view-independent base colour.
};

/**
 * @class GaussianSplat
 * @brief Loads a 3DGS `.ply` scene and prepares it for GPU alpha-composited rendering.
 *
 * ## Rendering approach (C3)
 * Each Gaussian is rendered as a screen-aligned quad (two triangles, four
 * vertices).  The vertex shader expands a single "point" into a quad whose
 * size and shape are determined by the projected 2D covariance.  The fragment
 * shader evaluates the Gaussian's opacity at each pixel and alpha-blends it.
 * Splats must be sorted back-to-front before drawing.
 *
 * ## Ownership model
 * - **Owns:** `VkBuffer` (GPU storage buffer), `VmaAllocation`.
 * - **Does NOT own:** the CPU `m_gaussians` array persists so sortByDepth()
 *   can re-sort each frame without re-uploading.
 *
 * ## Usage sketch (C3)
 * @code
 *   GaussianSplat gs;
 *   gs.loadPly("assets/garden.ply");
 *   gs.uploadToGPU(ctx);
 *   // render loop:
 *   gs.sortByDepth(camera.viewMatrix());
 *   gs.draw(cmd);
 *   // teardown:
 *   gs.destroy(ctx);
 * @endcode
 */
class GaussianSplat
{
public:
    // =========================================================================
    // Lifecycle
    // =========================================================================

    /**
     * @brief Parses a 3DGS `.ply` file into the CPU-side `m_gaussians` array.
     *
     * The `.ply` format has a plain-text header declaring property names and
     * types, followed by a binary block of tightly-packed vertex records.
     * This function reads the header to locate the byte offsets of each
     * required property, then reads the binary block into `GaussianPoint`
     * structs.  Higher-degree SH bands beyond degree 0 are currently ignored.
     *
     * @param  plyPath  Filesystem path to the trained `.ply` splat file.
     * @return true     File parsed successfully; `m_gaussians` is populated.
     * @return false    File not found, not a valid `.ply`, or missing required
     *                  properties (position, opacity, scale, rotation, shDC).
     *
     * @note  Implementation deferred to C3.
     */
    bool loadPly(const std::string& plyPath);

    /**
     * @brief Uploads `m_gaussians` to a device-local GPU storage buffer.
     *
     * Uses the same staging buffer pattern as Mesh::upload():
     *  1. Allocate a host-visible staging buffer.
     *  2. `memcpy` the GaussianPoint array into it.
     *  3. Submit `vkCmdCopyBuffer` → device-local storage buffer.
     *  4. Destroy the staging buffer.
     *
     * The storage buffer is bound as a shader storage buffer (SSBO) in the
     * splat vertex shader, which reads each Gaussian's attributes by instance
     * index.
     *
     * @param  ctx  Initialised VulkanContext.
     * @return true on success.
     *
     * @note  Implementation deferred to C3.
     */
    bool uploadToGPU(const VulkanContext& ctx);

    /**
     * @brief Destroys the GPU storage buffer and its VMA allocation.
     * @param ctx  The same VulkanContext passed to uploadToGPU().
     *
     * @note  Implementation deferred to C3.
     */
    void destroy(const VulkanContext& ctx);

    // =========================================================================
    // Render-time
    // =========================================================================

    /**
     * @brief Sorts `m_gaussians` back-to-front relative to the camera.
     *
     * Projects each Gaussian's position into camera space and sorts by the
     * resulting z-depth using `std::sort` (or `std::stable_sort` to preserve
     * order for equal depths).  This sort must run every frame as the camera
     * moves.
     *
     * A GPU-side radix sort (via a compute shader) is the stretch-goal
     * optimisation — it avoids the CPU→GPU data round-trip.
     *
     * @param viewMatrix  The current camera view matrix (world → camera space).
     *                    The projected depth of Gaussian i is:
     *                    `z = (viewMatrix * vec4(position, 1)).z`.
     *
     * @note  Implementation deferred to C3.
     */
    void sortByDepth(const glm::mat4& viewMatrix);

    /**
     * @brief Records the instanced splat draw call into a command buffer.
     *
     * Binds the GPU storage buffer and issues:
     * @code
     *   vkCmdDraw(cmd, 4 * splatCount(), 1, 0, 0);
     * @endcode
     * The vertex shader uses `gl_VertexIndex` to address the storage buffer
     * (`instanceID = gl_VertexIndex / 4`) and expands each Gaussian into a
     * screen-aligned quad (4 vertices per Gaussian, two triangles).
     *
     * Alpha blending must be enabled on the pipeline (C3 pipeline variant).
     * Splats must already be sorted back-to-front by sortByDepth().
     *
     * @param cmd  Active command buffer in recording state.
     *
     * @note  Implementation deferred to C3.
     */
    void draw(VkCommandBuffer cmd) const;

    // =========================================================================
    // Accessors
    // =========================================================================

    /// @brief Number of Gaussian primitives currently loaded.
    /// Zero if loadPly() has not been called successfully yet.
    uint32_t splatCount() const
        { return static_cast<uint32_t>(m_gaussians.size()); }

private:
    /// CPU-side array of all Gaussians.  Re-sorted each frame by sortByDepth().
    std::vector<GaussianPoint> m_gaussians;

    /// Device-local GPU storage buffer holding the sorted Gaussian array.
    VkBuffer      m_gpuBuffer = VK_NULL_HANDLE;
    /// VMA allocation backing m_gpuBuffer.
    VmaAllocation m_gpuAlloc  = VK_NULL_HANDLE;
};

#endif // FYP_VULKAN_RENDERER_GAUSSIANSPLAT_H
