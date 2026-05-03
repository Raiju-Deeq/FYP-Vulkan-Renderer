/**
 * @file mesh.hpp
 * @brief CPU geometry data and device-local GPU vertex/index buffers.
 *
 * ## What a vertex buffer is
 * A vertex buffer is a GPU-side array of per-vertex data (position, normal,
 * UV).  The GPU reads from it during the vertex shader stage.  For maximum
 * performance the buffer lives in *device-local* memory (VRAM on a discrete
 * GPU) — the CPU cannot write directly to it.
 *
 * ## Staging buffer upload pattern
 * Because device-local memory is write-only from the CPU's perspective, the
 * upload path uses a two-buffer approach:
 *
 *  1. Create a **staging buffer** in host-visible memory (normal RAM).
 *  2. `memcpy` the vertex/index data into the staging buffer.
 *  3. Submit a `vkCmdCopyBuffer` command to transfer from staging → device-local.
 *  4. Destroy the staging buffer once the transfer completes.
 *
 * This pattern is used for both the vertex buffer and the index buffer.
 *
 * ## VMA (Vulkan Memory Allocator)
 * GPU memory management in raw Vulkan is complex — I would have to manually call
 * `vkAllocateMemory` and track alignment, heap types and sub-allocations.
 * VMA (GPUOpen::VulkanMemoryAllocator) handles all of this automatically.
 * I call `vmaCreateBuffer` instead of `vkCreateBuffer` + `vkAllocateMemory`,
 * and `vmaDestroyBuffer` instead of `vkDestroyBuffer` + `vkFreeMemory`.
 *
 * @author Mohamed Deeq Mohamed (P2884884)
 * @date   2026-03-27
 */

#ifndef FYP_VULKAN_RENDERER_MESH_HPP
#define FYP_VULKAN_RENDERER_MESH_HPP

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <glm/glm.hpp>
#include <cstdint>
#include <string>
#include <vector>

class VulkanContext;

/**
 * @struct Vertex
 * @brief Interleaved per-vertex attributes uploaded to the vertex buffer.
 *
 * The memory layout here must exactly match the GLSL vertex shader input
 * declarations (location indices and types):
 * @code
 *   layout(location = 0) in vec3 inPosition;
 *   layout(location = 1) in vec3 inNormal;
 *   layout(location = 2) in vec2 inTexCoord;
 * @endcode
 *
 * The binding description tells Vulkan the stride (sizeof(Vertex)) and the
 * input rate (per-vertex).  The attribute descriptions tell it the byte
 * offset and format of each field within the struct.
 */
struct Vertex
{
    glm::vec3 position;  ///< World-space XYZ position. Maps to location 0.
    glm::vec3 normal;    ///< Unit surface normal (XYZ).  Maps to location 1.
    glm::vec2 texCoord;  ///< UV texture coordinates in [0, 1].  Maps to location 2.
};

/**
 * @struct LoadedMesh
 * @brief CPU-side mesh data ready to upload into vertex and index buffers.
 */
struct LoadedMesh
{
    std::vector<Vertex>   vertices; ///< Interleaved position/normal/UV data.
    std::vector<uint32_t> indices;  ///< 32-bit indices used by vkCmdDrawIndexed.
};

/**
 * @brief Loads and triangulates an OBJ mesh into the renderer's CPU mesh format.
 *
 * Applies the model's fixed import-orientation transform while repacking OBJ
 * position, normal and UV streams into the interleaved Vertex layout.
 *
 * @param path Filesystem path to the OBJ file.
 * @param outMesh Receives transformed vertices and generated indices.
 * @return true if the OBJ contained drawable triangle geometry.
 */
bool loadObjMesh(const std::string& path, LoadedMesh& outMesh);

/**
 * @class Mesh
 * @brief I own GPU vertex and index buffers for a single drawable mesh.
 *
 * ## Ownership model
 * - **I own:** `VkBuffer` (vertex), `VkBuffer` (index), `VmaAllocation` ×2.
 * - The VmaAllocations are the memory backing the buffers — I must free
 *   them together with the buffers via `vmaDestroyBuffer`.
 *
 * ## Usage pattern
 * @code
 *   Mesh mesh;
 *   mesh.upload(vertices, indices);   // one-time setup
 *   // per-frame, inside command buffer recording:
 *   mesh.bind(cmd);
 *   mesh.draw(cmd);
 *   // teardown:
 *   mesh.destroy();
 * @endcode
 */
class Mesh
{
public:
    // =========================================================================
    // Lifecycle
    // =========================================================================

    /**
     * @brief Uploads CPU geometry to device-local GPU memory.
     *
     * Uses the staging buffer pattern (see file header):
     *  1. Allocate host-visible staging buffers (one for vertices, one for indices).
     *  2. `memcpy` data in.
     *  3. Submit `vkCmdCopyBuffer` via a one-time command buffer.
     *  4. `vkQueueSubmit` and wait for the transfer to complete.
     *  5. Destroy staging buffers.
     *
     * @param  ctx       Fully initialised VulkanContext with a valid VMA allocator.
     * @param  vertices  CPU-side array of Vertex structs.
     * @param  indices   CPU-side array of 32-bit indices into the vertex array.
     * @return true      on success (both buffers allocated and filled).
     * @return false     if any VMA allocation or Vulkan transfer fails.
     */
    bool upload(const VulkanContext&         ctx,
                const std::vector<Vertex>&   vertices,
                const std::vector<uint32_t>& indices);

    /**
     * @brief Frees vertex and index buffers and their VMA allocations.
     */
    void destroy(const VulkanContext& ctx);

    // =========================================================================
    // Render-time helpers
    // =========================================================================

    /**
     * @brief Binds the vertex and index buffers into a command buffer.
     *
     * Records two commands:
     *  - `vkCmdBindVertexBuffers` — sets the source of vertex data for
     *    subsequent draw calls (reads attributes according to the pipeline's
     *    vertex input binding descriptions).
     *  - `vkCmdBindIndexBuffer` — sets the index buffer (32-bit uint indices).
     *
     * @param cmd  Active command buffer in recording state.
     */
    void bind(VkCommandBuffer cmd) const;

    /**
     * @brief Records an indexed draw call into the command buffer.
     *
     * Issues `vkCmdDrawIndexed(cmd, m_indexCount, 1, 0, 0, 0)`.
     * Must be called after bind() in the same command buffer recording.
     *
     * @param cmd  Active command buffer (bind() must have been called first).
     */
    void draw(VkCommandBuffer cmd) const;

    // =========================================================================
    // Accessors
    // =========================================================================

    /// @brief Number of indices in the index buffer.
    /// Passed as the `indexCount` argument to `vkCmdDrawIndexed`.
    uint32_t indexCount()  const { return m_indexCount; }

    /// @brief Number of vertices in the vertex buffer.
    uint32_t vertexCount() const { return m_vertexCount; }

private:
    VkBuffer      m_vertexBuffer = VK_NULL_HANDLE; ///< Device-local vertex buffer (allocated by VMA).
    VmaAllocation m_vertexAlloc  = VK_NULL_HANDLE; ///< VMA allocation backing m_vertexBuffer.
    VkBuffer      m_indexBuffer  = VK_NULL_HANDLE; ///< Device-local index buffer (allocated by VMA).
    VmaAllocation m_indexAlloc   = VK_NULL_HANDLE; ///< VMA allocation backing m_indexBuffer.
    uint32_t      m_vertexCount  = 0;              ///< Element count of the vertex buffer.
    uint32_t      m_indexCount   = 0;              ///< Element count of the index buffer.
};

#endif // FYP_VULKAN_RENDERER_MESH_HPP
