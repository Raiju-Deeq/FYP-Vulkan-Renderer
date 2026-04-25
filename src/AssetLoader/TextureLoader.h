/**
 * @file TextureLoader.h
 * @brief stb_image wrapper that loads files into predictable RGBA8 pixels.
 */

#ifndef FYP_VULKAN_RENDERER_TEXTURELOADER_H
#define FYP_VULKAN_RENDERER_TEXTURELOADER_H

#include "AssetLoader/AssetData.h"

#include <string>

namespace AssetLoader::TextureLoader
{
/**
 * @brief Loads an image from disk and converts it to RGBA8.
 *
 * @param  path       Path to a texture file supported by stb_image.
 * @param  outTexture Receives pixel bytes and dimensions.
 * @return true       if the image was loaded and converted successfully.
 */
bool loadRgba8(const std::string& path, LoadedTexture& outTexture);
} // namespace AssetLoader::TextureLoader

#endif // FYP_VULKAN_RENDERER_TEXTURELOADER_H
