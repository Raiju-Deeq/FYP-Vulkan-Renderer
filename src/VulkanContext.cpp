/**
 * @file VulkanContext.cpp
 * @brief Implementation of VulkanContext — Vulkan instance / device setup.
 *
 * @author Mohamed Deeq Mohamed
 * @date   2026-03-27
 *
 * @todo  Week 2 — implement init() using vk-bootstrap (VkBootstrap::InstanceBuilder,
 *                 PhysicalDeviceSelector, DeviceBuilder).
 * @todo  Week 2 — implement destroy() tearing down device, surface, debug messenger
 *                 and instance in reverse order.
 */

#include "VulkanContext.h"

bool VulkanContext::init(GLFWwindow* /*window*/)
{
    // TODO: Week 2 — vk-bootstrap instance + device creation
    return false;
}

void VulkanContext::destroy()
{
    // TODO: Week 2 — destroy in reverse creation order
}
