/**
 * @file TextureLoader.cpp
 * @brief stb_image-backed texture loading for Milestone 2.
 */

#include "AssetLoader/TextureLoader.h"

#include <spdlog/spdlog.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace AssetLoader::TextureLoader
{
bool loadRgba8(const std::string& path, LoadedTexture& outTexture)
{
    int width = 0;
    int height = 0;
    int sourceChannels = 0;

    stbi_uc* pixels = stbi_load(path.c_str(), &width, &height, &sourceChannels, STBI_rgb_alpha);
    if (!pixels) {
        spdlog::error("TextureLoader: failed to load '{}': {}", path, stbi_failure_reason());
        return false;
    }

    const size_t byteCount = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
    outTexture.pixels.assign(pixels, pixels + byteCount);
    outTexture.width = width;
    outTexture.height = height;
    outTexture.channels = 4;
    outTexture.path = path;

    stbi_image_free(pixels);

    spdlog::info("TextureLoader: loaded '{}' ({}x{}, source channels={})",
                 path, width, height, sourceChannels);
    return true;
}
} // namespace AssetLoader::TextureLoader
