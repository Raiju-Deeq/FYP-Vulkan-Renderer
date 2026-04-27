/**
 * @file Mesh.cpp
 * @brief GPU vertex/index buffer upload and indexed draw recording.
 */

#include "Mesh.h"

#include "AssetLoader/GpuUploader.h"
#include "VulkanContext.h"

#include <spdlog/spdlog.h>

bool Mesh::upload(const VulkanContext&         ctx,
                  const std::vector<Vertex>&   vertices,
                  const std::vector<uint32_t>& indices)
{
    destroy(ctx);

    if (vertices.empty() || indices.empty()) {
        spdlog::error("Mesh: upload called with empty geometry");
        return false;
    }

    AssetLoader::BufferResource vertexResource{};
    if (!AssetLoader::GpuUploader::uploadBuffer(
            ctx,
            vertices.data(),
            static_cast<VkDeviceSize>(vertices.size() * sizeof(Vertex)),
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            vertexResource)) {
        spdlog::error("Mesh: vertex buffer upload failed");
        return false;
    }

    AssetLoader::BufferResource indexResource{};
    if (!AssetLoader::GpuUploader::uploadBuffer(
            ctx,
            indices.data(),
            static_cast<VkDeviceSize>(indices.size() * sizeof(uint32_t)),
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            indexResource)) {
        spdlog::error("Mesh: index buffer upload failed");
        AssetLoader::GpuUploader::destroyBuffer(ctx, vertexResource);
        return false;
    }

    m_vertexBuffer = vertexResource.buffer;
    m_vertexAlloc = vertexResource.allocation;
    m_indexBuffer = indexResource.buffer;
    m_indexAlloc = indexResource.allocation;
    m_vertexCount = static_cast<uint32_t>(vertices.size());
    m_indexCount = static_cast<uint32_t>(indices.size());

    spdlog::info("Mesh: uploaded {} vertices and {} indices", m_vertexCount, m_indexCount);
    return true;
}

void Mesh::destroy(const VulkanContext& ctx)
{
    if (m_vertexBuffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(ctx.allocator(), m_vertexBuffer, m_vertexAlloc);
        m_vertexBuffer = VK_NULL_HANDLE;
        m_vertexAlloc = VK_NULL_HANDLE;
    }

    if (m_indexBuffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(ctx.allocator(), m_indexBuffer, m_indexAlloc);
        m_indexBuffer = VK_NULL_HANDLE;
        m_indexAlloc = VK_NULL_HANDLE;
    }

    m_vertexCount = 0;
    m_indexCount = 0;
}

void Mesh::bind(VkCommandBuffer cmd) const
{
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &m_vertexBuffer, &offset);
    vkCmdBindIndexBuffer(cmd, m_indexBuffer, 0, VK_INDEX_TYPE_UINT32);
}

void Mesh::draw(VkCommandBuffer cmd) const
{
    vkCmdDrawIndexed(cmd, m_indexCount, 1, 0, 0, 0);
}
