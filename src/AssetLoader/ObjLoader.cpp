/**
 * @file ObjLoader.cpp
 * @brief tinyobjloader-backed OBJ parsing for Milestone 2.
 */

#include "AssetLoader/ObjLoader.h"

#include <spdlog/spdlog.h>
#include <tiny_obj_loader.h>

namespace AssetLoader::ObjLoader
{
bool load(const std::string& path, LoadedMesh& outMesh)
{
    tinyobj::ObjReaderConfig config{};
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
                    1.0f - attrib.texcoords[base + 1]
                };
            } else {
                vertex.texCoord = {0.0f, 0.0f};
            }

            outMesh.vertices.push_back(vertex);
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
} // namespace AssetLoader::ObjLoader
