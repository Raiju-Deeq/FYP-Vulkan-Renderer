---
date: 2026-04-02
tags: [vulkan, fyp, milestone-1, vulkan-context, implementation]
status: in-progress
---

# VulkanContext.cpp - Step-by-Step Implementation Guide

**Milestone:** M1 - Baseline Vulkan Pipeline
**Prerequisite:** [[Vulkan-Object-Chain]], [[Vulkan-M1-Deep-Dive]]
**Plan:** [[Week-1-Day-3-Plan]]
**Goal:** Implement `VulkanContext::init()` and `VulkanContext::destroy()` - the foundation every other module depends on

---

## Overview

The header (`VulkanContext.h`) is already complete - it defines the class, the private members, and the public API. We only need to implement the `.cpp` file. There are exactly **two functions** to write: `init()` and `destroy()`.

---

## How `init()` Works - Stage by Stage

### Stage 1: Create the VkInstance

This is where the Vulkan loader wakes up. We use vk-bootstrap's `InstanceBuilder` - a builder pattern class that collects our requirements and then calls `vkCreateInstance` for us.

**What we tell it:**
- Our app name (cosmetic, but shows in debug tools like RenderDoc)
- We need Vulkan API version 1.3.0
- We want validation layers if available (for error checking)
- We want the default debug messenger (prints validation errors to stderr)

**What comes back:** A `vkb::Instance` struct containing the raw `VkInstance` handle and the `VkDebugUtilsMessengerEXT` handle. We store both in our member variables.

**What can go wrong:** The system doesn't support Vulkan 1.3 (old drivers). The builder returns an error result - we log it and return false.

### Stage 2: Create the Window Surface

GLFW gives us a one-liner: `glfwCreateWindowSurface(instance, window, nullptr, &surface)`. This calls the platform-specific surface creation function under the hood (Xlib on Arch Linux, Win32 at DMU).

**Why this must be Stage 2:** The physical device selector in Stage 3 needs the surface to check "can this GPU present to this window?" Without the surface, it cannot verify presentation support.

### Stage 3: Select a Physical Device

This is where we tell vk-bootstrap what GPU features we need. It queries every GPU on the system and picks the best match.

**The two feature structs:** Vulkan 1.3 features and 1.2 features live in separate structs:
- `VkPhysicalDeviceVulkan13Features` - we set `dynamicRendering = VK_TRUE` and `synchronization2 = VK_TRUE`
- `VkPhysicalDeviceVulkan12Features` - we set `bufferDeviceAddress = VK_TRUE`

We pass these to the selector via `set_required_features_13()` and `set_required_features_12()`. Any GPU that does not support all three features gets rejected.

We also say `prefer_gpu_device_type(discrete)` - pick a dedicated GPU over integrated if both are available.

**What comes back:** A `vkb::PhysicalDevice` with the raw `VkPhysicalDevice` handle and the GPU name (which we log).

### Stage 4: Create the Logical Device

The `DeviceBuilder` takes the physical device from Stage 3 and creates a `VkDevice`. All the features we required during selection are automatically enabled - no extra configuration needed.

**What comes back:** A `vkb::Device` with the raw `VkDevice` handle.

### Stage 5: Get the Graphics Queue

We ask the `vkb::Device` for a graphics queue and its family index. These are two separate calls - `get_queue()` returns the `VkQueue`, `get_queue_index()` returns the `uint32_t` family index.

The Renderer will need both: the queue for submitting commands, the family index for creating the command pool.

---

## How `destroy()` Works

Exact reverse of creation order. Each handle is checked against `VK_NULL_HANDLE` before destruction so it is safe to call after partial init or multiple times:

1. Destroy the logical device (Stage 4 reverse)
2. Destroy the surface (Stage 2 reverse)
3. Destroy the debug messenger (Stage 1b reverse)
4. Destroy the instance (Stage 1a reverse)

`VkPhysicalDevice` and `VkQueue` are never destroyed - they are not owned handles, just references retrieved from the driver.

---

## Pseudocode

This maps 1:1 to the real code - the only difference is replacing pseudocode syntax with actual C++ and vk-bootstrap API calls.

### `init()`

```
FUNCTION init(window) -> bool:

    // ── Stage 1: Instance ──────────────────────────────────

    builder = new InstanceBuilder()
    builder.set_app_name("FYP Vulkan Renderer")
    builder.require_api_version(1, 3, 0)
    builder.request_validation_layers()       // enable if available
    builder.use_default_debug_messenger()     // prints errors to stderr

    result = builder.build()

    IF result failed:
        LOG_ERROR("Failed to create Vulkan instance: " + result.error_message)
        RETURN false

    vkbInstance      = result.value
    m_instance       = vkbInstance.instance
    m_debugMessenger = vkbInstance.debug_messenger

    // ── Stage 2: Surface ───────────────────────────────────

    status = glfwCreateWindowSurface(m_instance, window, null, &m_surface)

    IF status != VK_SUCCESS:
        LOG_ERROR("Failed to create window surface")
        RETURN false

    // ── Stage 3: Physical Device ───────────────────────────

    features13 = new Vulkan13Features{}          // all fields zero by default
    features13.dynamicRendering = TRUE
    features13.synchronization2 = TRUE

    features12 = new Vulkan12Features{}          // all fields zero by default
    features12.bufferDeviceAddress = TRUE

    selector = new PhysicalDeviceSelector(vkbInstance, m_surface)
    selector.set_minimum_version(1, 3)
    selector.set_required_features_13(features13)
    selector.set_required_features_12(features12)
    selector.prefer_gpu_device_type(DISCRETE)

    result = selector.select()

    IF result failed:
        LOG_ERROR("Failed to select a suitable GPU: " + result.error_message)
        RETURN false

    vkbPhysicalDevice = result.value
    m_physicalDevice  = vkbPhysicalDevice.physical_device
    LOG_INFO("Selected GPU: " + vkbPhysicalDevice.name)

    // ── Stage 4: Logical Device ────────────────────────────

    deviceBuilder = new DeviceBuilder(vkbPhysicalDevice)
    result = deviceBuilder.build()

    IF result failed:
        LOG_ERROR("Failed to create logical device: " + result.error_message)
        RETURN false

    vkbDevice = result.value
    m_device  = vkbDevice.device

    // ── Stage 5: Graphics Queue ────────────────────────────

    queueResult = vkbDevice.get_queue(GRAPHICS)

    IF queueResult failed:
        LOG_ERROR("Failed to get graphics queue: " + queueResult.error_message)
        RETURN false

    m_graphicsQueue = queueResult.value

    indexResult = vkbDevice.get_queue_index(GRAPHICS)

    IF indexResult failed:
        LOG_ERROR("Failed to get graphics queue family: " + indexResult.error_message)
        RETURN false

    m_graphicsQueueFamily = indexResult.value

    // ── Done ───────────────────────────────────────────────

    LOG_INFO("VulkanContext initialised successfully")
    RETURN true
```

### `destroy()`

```
FUNCTION destroy():

    // Reverse order of creation. Guard each with null check.

    IF m_device != NULL_HANDLE:
        vkDestroyDevice(m_device, null)
        m_device = NULL_HANDLE

    IF m_surface != NULL_HANDLE:
        vkDestroySurfaceKHR(m_instance, m_surface, null)
        m_surface = NULL_HANDLE

    IF m_debugMessenger != NULL_HANDLE:
        vkb::destroy_debug_utils_messenger(m_instance, m_debugMessenger)
        m_debugMessenger = NULL_HANDLE

    IF m_instance != NULL_HANDLE:
        vkDestroyInstance(m_instance, null)
        m_instance = NULL_HANDLE
```

---

## Pseudocode to Real C++ Mapping

| Pseudocode | Real C++ |
|-----------|----------|
| `new InstanceBuilder()` | `vkb::InstanceBuilder{}` |
| `result.failed` | `!result` |
| `result.error_message` | `result.error().message()` |
| `result.value` | `result.value()` |
| `new Vulkan13Features{}` | `VkPhysicalDeviceVulkan13Features{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES }` |
| `new Vulkan12Features{}` | `VkPhysicalDeviceVulkan12Features{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES }` |
| `TRUE` | `VK_TRUE` |
| `DISCRETE` | `vkb::PreferredDeviceType::discrete` |
| `GRAPHICS` | `vkb::QueueType::graphics` |
| `NULL_HANDLE` | `VK_NULL_HANDLE` |
| `LOG_ERROR(msg)` | `spdlog::error(msg)` |
| `LOG_INFO(msg)` | `spdlog::info(msg)` |

---

## Required Includes

```cpp
#include "VulkanContext.h"   // already present in stub
#include <VkBootstrap.h>     // vkb::InstanceBuilder, PhysicalDeviceSelector, DeviceBuilder
#include <GLFW/glfw3.h>      // glfwCreateWindowSurface
#include <spdlog/spdlog.h>   // spdlog::info, spdlog::error
```

---

*FYP - Vulkan Renderer in C++20 · Mohamed Deeq Mohamed · P2884884 · De Montfort University*
