/**
 * @file Mesh.cpp
 * @brief Implementation of Mesh - GPU buffer upload and draw recording.
 *
 * @author Mohamed Deeq Mohamed
 * @date   2026-03-27
 *
 * @todo  Week 3 - implement upload() using a VMA staging buffer +
 *                 one-time submit vkCmdCopyBuffer.
 * @todo  Week 3 - implement bind() / draw() with vkCmdBindVertexBuffers,
 *                 vkCmdBindIndexBuffer, vkCmdDrawIndexed.
 * @todo  Week 3 - implement destroy() freeing VMA allocations.
 */

#include "Mesh.h"

bool Mesh::upload(const std::vector<Vertex>&   /*vertices*/,
                  const std::vector<uint32_t>& /*indices*/)
{
    // TODO: Week 3 - VMA staging buffer + device-local transfer
    return false;
}

void Mesh::destroy()
{
    // TODO: Week 3 - vmaDestroyBuffer x2
}

void Mesh::bind(VkCommandBuffer /*cmd*/) const
{
    // TODO: Week 3 - vkCmdBindVertexBuffers + vkCmdBindIndexBuffer
}

void Mesh::draw(VkCommandBuffer /*cmd*/) const
{
    // TODO: Week 3 - vkCmdDrawIndexed
}
