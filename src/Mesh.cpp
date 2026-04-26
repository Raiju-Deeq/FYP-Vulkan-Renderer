/**
 * @file Mesh.cpp
 * @brief Stub implementation of Mesh — GPU buffer upload and draw recording.
 *
 * ## Implementation plan (Week 3 — Milestone 2)
 *
 * Use `AssetLoader::GpuUploader::uploadBuffer` for both vertex and index uploads —
 * it handles staging buffer creation, memcpy, vkCmdCopyBuffer, and cleanup
 * internally.  Store the returned `BufferResource` into m_vertexAlloc / m_indexAlloc,
 * or extract the handles and call `vmaDestroyBuffer` in destroy().
 *
 * ```cpp
 * // upload()
 * GpuUploader::uploadBuffer(ctx, vertices.data(),
 *     vertices.size() * sizeof(Vertex),
 *     VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertResource);
 * m_vertexBuffer = vertResource.buffer;
 * m_vertexAlloc  = vertResource.allocation;
 *
 * GpuUploader::uploadBuffer(ctx, indices.data(),
 *     indices.size() * sizeof(uint32_t),
 *     VK_BUFFER_USAGE_INDEX_BUFFER_BIT, idxResource);
 * m_indexBuffer = idxResource.buffer;
 * m_indexAlloc  = idxResource.allocation;
 *
 * // bind()
 * VkDeviceSize offset = 0;
 * vkCmdBindVertexBuffers(cmd, 0, 1, &m_vertexBuffer, &offset);
 * vkCmdBindIndexBuffer(cmd, m_indexBuffer, 0, VK_INDEX_TYPE_UINT32);
 *
 * // draw()
 * vkCmdDrawIndexed(cmd, m_indexCount, 1, 0, 0, 0);
 *
 * // destroy()
 * vmaDestroyBuffer(ctx.allocator(), m_vertexBuffer, m_vertexAlloc);
 * vmaDestroyBuffer(ctx.allocator(), m_indexBuffer,  m_indexAlloc);
 * ```
 *
 * @author Mohamed Deeq Mohamed (P2884884)
 * @date   2026-03-27
 */

#include "Mesh.h"

bool Mesh::upload(const std::vector<Vertex>&   /*vertices*/,
                  const std::vector<uint32_t>& /*indices*/)
{
    // TODO: Week 3 — VMA staging buffer → vkCmdCopyBuffer → device-local buffer
    return false;
}

void Mesh::destroy()
{
    // TODO: Week 3 — vmaDestroyBuffer(allocator, m_vertexBuffer, m_vertexAlloc)
    //                vmaDestroyBuffer(allocator, m_indexBuffer,  m_indexAlloc)
}

void Mesh::bind(VkCommandBuffer /*cmd*/) const
{
    // TODO: Week 3 — vkCmdBindVertexBuffers + vkCmdBindIndexBuffer
}

void Mesh::draw(VkCommandBuffer /*cmd*/) const
{
    // TODO: Week 3 — vkCmdDrawIndexed(cmd, m_indexCount, 1, 0, 0, 0)
}
