/**
 * @file TextureLoader.cpp
 * @brief stb_image-backed texture loading for Milestone 2.
 */

#include "AssetLoader/TextureLoader.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <fstream>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace AssetLoader::TextureLoader
{
namespace
{
bool loadAsciiPpm(const std::string& path, LoadedTexture& outTexture)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    std::string magic;
    int width = 0;
    int height = 0;
    int maxValue = 0;
    file >> magic >> width >> height >> maxValue;
    if (magic != "P3" || width <= 0 || height <= 0 || maxValue <= 0) {
        return false;
    }

    outTexture.pixels.clear();
    outTexture.pixels.reserve(static_cast<size_t>(width) * static_cast<size_t>(height) * 4);

    for (int i = 0; i < width * height; ++i) {
        int r = 0;
        int g = 0;
        int b = 0;
        file >> r >> g >> b;
        if (!file) {
            return false;
        }

        const auto scale = [maxValue](int value) {
            value = std::max(0, std::min(value, maxValue));
            return static_cast<uint8_t>((value * 255) / maxValue);
        };

        outTexture.pixels.push_back(scale(r));
        outTexture.pixels.push_back(scale(g));
        outTexture.pixels.push_back(scale(b));
        outTexture.pixels.push_back(255);
    }

    outTexture.width = width;
    outTexture.height = height;
    outTexture.channels = 4;
    outTexture.path = path;
    return true;
}
} // namespace

bool loadRgba8(const std::string& path, LoadedTexture& outTexture)
{
    int width = 0;
    int height = 0;
    int sourceChannels = 0;

    // Force 4-channel RGBA8 output regardless of source format so the Vulkan
    // upload path can use VK_FORMAT_R8G8B8A8_SRGB without per-texture branching.
    stbi_uc* pixels = stbi_load(path.c_str(), &width, &height, &sourceChannels, STBI_rgb_alpha);
    if (!pixels) {
        if (loadAsciiPpm(path, outTexture)) {
            spdlog::info("TextureLoader: loaded '{}' ({}x{}, ASCII PPM)",
                         path, outTexture.width, outTexture.height);
            return true;
        }

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
