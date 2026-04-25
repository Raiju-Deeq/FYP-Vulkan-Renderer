/**
 * @file ObjLoader.h
 * @brief tinyobjloader wrapper that converts one OBJ file into LoadedMesh.
 */

#ifndef FYP_VULKAN_RENDERER_OBJLOADER_H
#define FYP_VULKAN_RENDERER_OBJLOADER_H

#include "AssetLoader/AssetData.h"

#include <string>

namespace AssetLoader::ObjLoader
{
/**
 * @brief Loads an OBJ mesh from disk using tinyobjloader.
 *
 * The output is intentionally renderer-ready rather than engine-general:
 * triangles only, one flattened vertex stream, one 32-bit index stream, and
 * missing normals/UVs filled with safe defaults so M2 can get something on
 * screen before adding full asset validation.
 *
 * @param  path    Path to the `.obj` file.
 * @param  outMesh Receives CPU-side vertex and index arrays.
 * @return true    if at least one triangle was loaded.
 */
bool load(const std::string& path, LoadedMesh& outMesh);
} // namespace AssetLoader::ObjLoader

#endif // FYP_VULKAN_RENDERER_OBJLOADER_H
