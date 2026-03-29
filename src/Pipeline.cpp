/**
 * @file Pipeline.cpp
 * @brief Implementation of Pipeline — shader loading and pipeline creation.
 *
 * @author Mohamed Deeq Mohamed
 * @date   2026-03-27
 *
 * @todo  Week 2 — implement loadSpv() using std::ifstream (binary mode).
 * @todo  Week 2 — implement createShaderModule() via vkCreateShaderModule.
 * @todo  Week 2 — implement init() building VkGraphicsPipelineCreateInfo
 *                 with VkPipelineRenderingCreateInfoKHR for Dynamic Rendering.
 */

#include "Pipeline.h"
#include "VulkanContext.h"
#include <fstream>

bool Pipeline::init(const VulkanContext& /*ctx*/,
                    const std::string&   /*vertSpvPath*/,
                    const std::string&   /*fragSpvPath*/,
                    VkFormat             /*colourFormat*/)
{
    // TODO: Week 2 — load SPV, create shader modules, build pipeline
    return false;
}

void Pipeline::destroy(const VulkanContext& /*ctx*/)
{
    // TODO: Week 2 — vkDestroyPipeline, vkDestroyPipelineLayout,
    //                vkDestroyDescriptorSetLayout
}

// static
std::vector<uint32_t> Pipeline::loadSpv(const std::string& /*path*/)
{
    // TODO: Week 2 — open file in binary mode, read into uint32_t vector
    return {};
}

// static
VkShaderModule Pipeline::createShaderModule(VkDevice /*device*/,
                                             const std::vector<uint32_t>& /*code*/)
{
    // TODO: Week 2 — vkCreateShaderModule
    return VK_NULL_HANDLE;
}
