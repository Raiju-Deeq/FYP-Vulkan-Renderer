/**
 * @file AssetData.h
 * @brief Shared CPU-side asset containers used by the Milestone 2 loader path.
 *
 * AssetLoader deliberately separates CPU parsing from GPU upload:
 *  - ObjLoader fills `LoadedMesh`.
 *  - TextureLoader fills `LoadedTexture`.
 *  - GpuUploader turns those plain CPU containers into Vulkan/VMA resources.
 *
 * Keeping these as simple structs makes the M2 handoff clean: the loading code
 * can be tested without a Vulkan device, and the upload code does not need to
 * know anything about OBJ or PNG/JPEG file formats.
 */

#ifndef FYP_VULKAN_RENDERER_ASSETDATA_H
#define FYP_VULKAN_RENDERER_ASSETDATA_H

#include "Mesh.h"

#include <cstdint>
#include <string>
#include <vector>

namespace AssetLoader
{
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
 * @struct LoadedTexture
 * @brief CPU-side RGBA8 texture data returned by stb_image.
 *
 * Pixels are always converted to 4 channels so the Vulkan upload path can use
 * one predictable format: `VK_FORMAT_R8G8B8A8_SRGB`.
 */
struct LoadedTexture
{
    std::vector<uint8_t> pixels;   ///< Tightly packed RGBA8 pixels.
    int                 width = 0; ///< Width in pixels.
    int                 height = 0; ///< Height in pixels.
    int                 channels = 4; ///< Always 4 after loading.
    std::string         path;      ///< Source path, useful for logging/debug UI.
};
} // namespace AssetLoader

#endif // FYP_VULKAN_RENDERER_ASSETDATA_H
