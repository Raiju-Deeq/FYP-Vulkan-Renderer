/**
 * @file Mesh.h
 * @brief CPU-side mesh data and GPU vertex/index buffer management.
 *
 * Mesh stores raw vertex and index data on the CPU, then uploads it to
 * device-local GPU memory via a staging buffer (VMA).  It owns the
 * VkBuffer handles and VmaAllocation objects for its lifetime.
 *
 * @author Mohamed Deeq Mohamed
 * @date   2026-03-27
 */

#ifndef FYP_VULKAN_RENDERER_MESH_H
#define FYP_VULKAN_RENDERER_MESH_H

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>

/**
 * @struct Vertex
 * @brief Interleaved per-vertex attributes sent to the vertex shader.
 *
 * Matches the layout declared in the GLSL vertex shader:
 * @code
 *   layout(location=0) in vec3 inPosition;
 *   layout(location=1) in vec3 inNormal;
 *   layout(location=2) in vec2 inTexCoord;
 * @endcode
 */
struct Vertex
{
    glm::vec3 position;  ///< World-space position (XYZ).
    glm::vec3 normal;    ///< Surface normal (XYZ), assumed unit length.
    glm::vec2 texCoord;  ///< UV texture coordinates (0-1 range).
};

/**
 * @class Mesh
 * @brief Owns GPU vertex and index buffers for a single drawable object.
 *
 * Upload geometry once with upload(), then call bind() and draw() inside
 * a command buffer recording.  Call destroy() when the mesh is no longer
 * needed to free GPU memory.
 */
class Mesh
{
public:
    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------

    /**
     * @brief Uploads vertex and index data to device-local GPU memory.
     *
     * Internally creates a host-visible staging buffer, copies data into
     * it, then submits a one-time transfer command to copy to
     * device-local buffers.
     *
     * @param  vertices  CPU-side vertex array.
     * @param  indices   CPU-side index array (uint32_t).
     * @return true      on success.
     * @return false     if buffer creation or transfer fails.
     */
    bool upload(const std::vector<Vertex>&   vertices,
                const std::vector<uint32_t>& indices);

    /**
     * @brief Frees the vertex buffer, index buffer and VMA allocations.
     */
    void destroy();

    // -------------------------------------------------------------------------
    // Render-time helpers
    // -------------------------------------------------------------------------

    /**
     * @brief Binds the vertex and index buffers into a command buffer.
     * @param cmd  Active command buffer in recording state.
     */
    void bind(VkCommandBuffer cmd) const;

    /**
     * @brief Records an indexed draw call into the command buffer.
     * @param cmd  Active command buffer (bind() must have been called first).
     */
    void draw(VkCommandBuffer cmd) const;

    // -------------------------------------------------------------------------
    // Accessors
    // -------------------------------------------------------------------------

    /// @brief Returns the number of indices (used for vkCmdDrawIndexed count).
    uint32_t indexCount()  const { return m_indexCount; }

    /// @brief Returns the number of vertices.
    uint32_t vertexCount() const { return m_vertexCount; }

private:
    VkBuffer m_vertexBuffer = VK_NULL_HANDLE; ///< Device-local vertex buffer.
    VkBuffer m_indexBuffer  = VK_NULL_HANDLE; ///< Device-local index buffer.
    uint32_t m_vertexCount  = 0;              ///< Number of vertices.
    uint32_t m_indexCount   = 0;              ///< Number of indices.
};

#endif // FYP_VULKAN_RENDERER_MESH_H
