/**
 * @file Mesh.cpp
 * @brief Stub implementation of Mesh — GPU buffer upload and draw recording.
 *
 * ## Implementation plan (Week 3 — Milestone 2)
 *
 * ### upload()
 *  1. I calculate the byte sizes for the vertex and index arrays.
 *  2. I create a *staging* buffer in host-visible memory (CPU-writable) via VMA:
 *     ```cpp
 *     VmaAllocationCreateInfo stagingAllocInfo{};
 *     stagingAllocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
 *     vmaCreateBuffer(allocator, &bufferInfo, &stagingAllocInfo,
 *                     &stagingBuffer, &stagingAlloc, nullptr);
 *     ```
 *  3. Map the staging buffer and `memcpy` the CPU data in:
 *     ```cpp
 *     void* data;
 *     vmaMapMemory(allocator, stagingAlloc, &data);
 *     memcpy(data, vertices.data(), byteSize);
 *     vmaUnmapMemory(allocator, stagingAlloc);
 *     ```
 *  4. Create the device-local vertex buffer (GPU VRAM, not CPU-writable):
 *     ```cpp
 *     allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
 *     vmaCreateBuffer(allocator, &bufferInfo, &allocInfo,
 *                     &m_vertexBuffer, &m_vertexAlloc, nullptr);
 *     ```
 *  5. Submit a one-time command buffer with `vkCmdCopyBuffer` and wait idle.
 *  6. Destroy the staging buffer via `vmaDestroyBuffer`.
 *  7. Repeat steps 2–6 for the index buffer.
 *
 * ### bind()
 * ```cpp
 * VkBuffer     buffers[] = { m_vertexBuffer };
 * VkDeviceSize offsets[] = { 0 };
 * vkCmdBindVertexBuffers(cmd, 0, 1, buffers, offsets);
 * vkCmdBindIndexBuffer(cmd, m_indexBuffer, 0, VK_INDEX_TYPE_UINT32);
 * ```
 *
 * ### draw()
 * ```cpp
 * vkCmdDrawIndexed(cmd, m_indexCount, 1, 0, 0, 0);
 * // args: indexCount, instanceCount=1, firstIndex=0, vertexOffset=0, firstInstance=0
 * ```
 *
 * ### destroy()
 * ```cpp
 * vmaDestroyBuffer(allocator, m_vertexBuffer, m_vertexAlloc);
 * vmaDestroyBuffer(allocator, m_indexBuffer,  m_indexAlloc);
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
