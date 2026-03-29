/**
 * @file GaussianSplat.cpp
 * @brief Implementation of GaussianSplat — .ply loading and splat rendering.
 *
 * @author Mohamed Deeq Mohamed
 * @date   2026-03-28
 *
 * @note  Implementation is deferred to M6 (stretch-goal week).
 *
 * @todo  M6 — implement loadPly() parsing .ply binary_little_endian
 *             format produced by the official 3DGS training code.
 * @todo  M6 — implement uploadToGPU() copying m_gaussians into a
 *             VMA device-local VkBuffer via staging.
 * @todo  M6 — implement sortByDepth() using std::sort on projected depth.
 * @todo  M6 — implement draw() recording vkCmdDraw for instanced quads.
 */

#include "GaussianSplat.h"
#include "VulkanContext.h"

bool GaussianSplat::loadPly(const std::string& /*plyPath*/)
{
    // TODO: M6 — parse .ply header and binary data into m_gaussians
    return false;
}

bool GaussianSplat::uploadToGPU(const VulkanContext& /*ctx*/)
{
    // TODO: M6 — staging buffer + vkCmdCopyBuffer to device-local storage
    return false;
}

void GaussianSplat::destroy(const VulkanContext& /*ctx*/)
{
    // TODO: M6 — vmaDestroyBuffer
}

void GaussianSplat::sortByDepth(const glm::mat4& /*viewMatrix*/)
{
    // TODO: M6 — std::sort / radix sort on projected z depth
}

void GaussianSplat::draw(VkCommandBuffer /*cmd*/) const
{
    // TODO: M6 — vkCmdDraw instanced quad splats
}
