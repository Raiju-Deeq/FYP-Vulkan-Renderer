/**
 * @file mesh.cpp
 * @brief GPU vertex/index buffer upload and indexed draw recording.
 */

#include "mesh.hpp"

#include "gpu_buffer.hpp"
#include "vulkan_context.hpp"

#include <spdlog/spdlog.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

bool loadObjMesh(const std::string& path, LoadedMesh& outMesh)
{
    tinyobj::ObjReaderConfig config{};
    // I force triangles so the draw path can stay simple: one indexed
    // triangle list, no topology branching.
    config.triangulate = true;

    tinyobj::ObjReader reader;
    if (!reader.ParseFromFile(path, config)) {
        if (!reader.Error().empty()) {
            spdlog::error("ObjLoader: {}", reader.Error());
        }
        return false;
    }

    if (!reader.Warning().empty()) {
        spdlog::warn("ObjLoader: {}", reader.Warning());
    }

    const tinyobj::attrib_t& attrib = reader.GetAttrib();
    const auto& shapes = reader.GetShapes();

    outMesh.vertices.clear();
    outMesh.indices.clear();

    // I repack tinyobj's separate position/normal/UV arrays into my interleaved
    // Vertex layout, which is exactly what the Vulkan vertex input expects.
    for (const tinyobj::shape_t& shape : shapes) {
        for (const tinyobj::index_t& index : shape.mesh.indices) {
            Vertex vertex{};

            if (index.vertex_index >= 0) {
                const size_t base = static_cast<size_t>(index.vertex_index) * 3;
                vertex.position = {
                    attrib.vertices[base + 0],
                    attrib.vertices[base + 1],
                    attrib.vertices[base + 2]
                };
            }

            if (index.normal_index >= 0) {
                const size_t base = static_cast<size_t>(index.normal_index) * 3;
                vertex.normal = {
                    attrib.normals[base + 0],
                    attrib.normals[base + 1],
                    attrib.normals[base + 2]
                };
            } else {
                vertex.normal = {0.0f, 1.0f, 0.0f};
            }

            if (index.texcoord_index >= 0) {
                const size_t base = static_cast<size_t>(index.texcoord_index) * 2;
                vertex.texCoord = {
                    attrib.texcoords[base + 0],
                    1.0f - attrib.texcoords[base + 1] // OBJ V=0 is bottom; Vulkan V=0 is top.
                };
            } else {
                vertex.texCoord = {0.0f, 0.0f};
            }

            outMesh.vertices.push_back(vertex);
            // I keep one output vertex per OBJ index for now. Deduplication can
            // be added later, but this is simple and correct for the milestone.
            outMesh.indices.push_back(static_cast<uint32_t>(outMesh.indices.size()));
        }
    }

    if (outMesh.vertices.empty() || outMesh.indices.empty()) {
        spdlog::error("ObjLoader: '{}' did not contain drawable triangles", path);
        return false;
    }

    spdlog::info("ObjLoader: loaded '{}' ({} vertices, {} indices)",
                 path, outMesh.vertices.size(), outMesh.indices.size());
    return true;
}

bool Mesh::upload(const VulkanContext&         ctx,
                  const std::vector<Vertex>&   vertices,
                  const std::vector<uint32_t>& indices)
{
    // Re-uploading a mesh should replace the old GPU buffers, not leak them.
    destroy(ctx);

    if (vertices.empty() || indices.empty()) {
        spdlog::error("Mesh: upload called with empty geometry");
        return false;
    }

    GpuBuffer::BufferResource vertexResource{};
    if (!GpuBuffer::uploadBuffer(
            ctx,
            vertices.data(),
            static_cast<VkDeviceSize>(vertices.size() * sizeof(Vertex)),
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            vertexResource)) {
        spdlog::error("Mesh: vertex buffer upload failed");
        return false;
    }

    GpuBuffer::BufferResource indexResource{};
    if (!GpuBuffer::uploadBuffer(
            ctx,
            indices.data(),
            static_cast<VkDeviceSize>(indices.size() * sizeof(uint32_t)),
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            indexResource)) {
        spdlog::error("Mesh: index buffer upload failed");
        GpuBuffer::destroyBuffer(ctx, vertexResource);
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
    // I bind both buffers together so the following draw call can stay tiny.
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &m_vertexBuffer, &offset);
    vkCmdBindIndexBuffer(cmd, m_indexBuffer, 0, VK_INDEX_TYPE_UINT32);
}

void Mesh::draw(VkCommandBuffer cmd) const
{
    vkCmdDrawIndexed(cmd, m_indexCount, 1, 0, 0, 0);
}
