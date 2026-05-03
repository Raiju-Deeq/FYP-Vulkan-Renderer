/**
 * @file vulkan_context.cpp
 * @brief Implementation of VulkanContext: Vulkan instance, device and queue bootstrap.
 *
 * ## Reading guide
 * This file is deliberately linear — read init() top to bottom and I'll walk
 * you through exactly the sequence every Vulkan application must perform
 * before drawing a single pixel. Each stage has a comment block explaining
 * *why* it exists and what would break if I skipped it or reordered it.
 *
 * I use **vk-bootstrap** (github.com/charles-lunarg/vk-bootstrap) to handle
 * the query-then-select boilerplate (enumerating GPUs, checking extensions,
 * etc.). Without it, each stage would require 30–80 lines of setup code.
 * vk-bootstrap doesn't hide anything from me — it just automates the grunt
 * work and hands me the raw Vulkan handles at the end.
 *
 * @author Mohamed Deeq Mohamed (P2884884)
 * @date   2026-04-02
 * @see    vulkan_context.hpp for the class interface and ownership rules.
 * @see    https://github.com/charles-lunarg/vk-bootstrap
 */

#include "vulkan_context.hpp"

#include <VkBootstrap.h>   // vk-bootstrap helper types (InstanceBuilder, etc.)
#include <GLFW/glfw3.h>    // glfwGetRequiredInstanceExtensions, glfwCreateWindowSurface
#include <spdlog/spdlog.h> // structured logging

// =============================================================================
// init()
// =============================================================================

/// @details
/// **Stage overview and dependency chain:**
/// ```
///  Instance  →  Surface  →  PhysicalDevice  →  Device  →  Queue
/// ```
/// Each arrow means "must exist before the next step". Vulkan enforces this
/// at runtime — calling out-of-order results in validation errors or crashes.
///
/// **Why does Surface come before PhysicalDevice selection?**
/// I need to pass the surface to the physical device selector so it can verify
/// that my chosen GPU can actually *present* to this window — not all GPUs
/// can present to every surface type. Creating the surface first lets
/// vk-bootstrap reject GPUs that support graphics but cannot present.
bool VulkanContext::init(GLFWwindow* window)
{
    // ── Stage 1: Vulkan Instance ───────────────────────────────────────────
    // The VkInstance is my application's registration with the Vulkan loader.
    // Think of it as opening a connection to the driver stack. Nothing else
    // can be created until the instance exists.
    //
    // GLFW extensions: GLFW needs certain instance-level extensions to create
    // a window surface. On Linux these include VK_KHR_surface plus either
    // VK_KHR_xcb_surface (X11) or VK_KHR_wayland_surface (Wayland). On
    // Windows it's VK_KHR_win32_surface. GLFW tells me exactly which ones it
    // needs via glfwGetRequiredInstanceExtensions — I must ask for them before
    // building the instance, or surface creation will fail later.
    uint32_t     glfwExtCount = 0;
    const char** glfwExts     = glfwGetRequiredInstanceExtensions(&glfwExtCount);
    if (!glfwExts) {
        spdlog::error("VulkanContext: glfwGetRequiredInstanceExtensions returned null — "
                      "is a Vulkan-capable display server (X11/Wayland) running?");
        return false;
    }

    vkb::InstanceBuilder builder;
    auto instResult = builder
        .set_app_name("FYP Vulkan Renderer")
        .require_api_version(1, 3, 0)       // I refuse GPUs/drivers older than Vulkan 1.3
        .request_validation_layers()         // Enable Khronos validation (debug only)
        .use_default_debug_messenger()       // Print validation errors to stderr
        .enable_extensions(                  // Add GLFW's required windowing extensions
            std::vector<const char*>(glfwExts, glfwExts + glfwExtCount))
        .build();

    if (!instResult) {
        spdlog::error("Failed to create Vulkan instance: {}", instResult.error().message());
        return false;
    }

    // vkb::Instance wraps the raw handle plus the debug messenger.
    // I store both so destroy() can clean them up individually.
    vkb::Instance vkbInstance = instResult.value();
    m_instance      = vkbInstance.instance;
    m_debugMessenger = vkbInstance.debug_messenger;

    // ── Stage 2: Window Surface ────────────────────────────────────────────
    // VkSurfaceKHR is an abstract, platform-neutral handle for my OS window
    // that Vulkan will render into. Under the hood, GLFW calls the correct
    // platform-specific function:
    //   Linux X11     → vkCreateXlibSurfaceKHR / vkCreateXcbSurfaceKHR
    //   Linux Wayland → vkCreateWaylandSurfaceKHR
    //   Windows       → vkCreateWin32SurfaceKHR
    //
    // I create the surface at this stage — before GPU selection — so the
    // physical device selector can verify presentation support.
    if (glfwCreateWindowSurface(m_instance, window, nullptr, &m_surface) != VK_SUCCESS) {
        spdlog::error("Failed to create window surface");
        return false;
    }

    // ── Stage 3: Physical Device Selection ────────────────────────────────
    // A VkPhysicalDevice is a descriptor for one GPU in my system. I don't
    // create or destroy it — the driver owns it and it becomes invalid when
    // the instance is destroyed.
    //
    // I enumerate all available GPUs and select the best one that meets my
    // minimum requirements. The required features live in two separate structs
    // because they were promoted to core in different Vulkan versions:
    //
    //   VkPhysicalDeviceVulkan13Features:
    //   - dynamicRendering  → lets me record render commands without pre-baking
    //                         a VkRenderPass. This is the approach I use throughout.
    //   - synchronization2  → cleaner image layout barrier API with granular
    //                         pipeline stage masks (VkImageMemoryBarrier2).
    //
    //   VkPhysicalDeviceVulkan12Features:
    //   - bufferDeviceAddress → keeps the future Gaussian splatting path possible
    //                           without changing device creation later.
    VkPhysicalDeviceVulkan13Features features13{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES
    };
    features13.dynamicRendering = VK_TRUE;
    features13.synchronization2 = VK_TRUE;

    VkPhysicalDeviceVulkan12Features features12{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES
    };
    features12.bufferDeviceAddress = VK_TRUE;

    // The selector needs both the instance wrapper (to iterate GPUs) and the
    // surface (to test presentation support on each candidate).
    vkb::PhysicalDeviceSelector selector(vkbInstance, m_surface);
    auto physResult = selector
        .set_minimum_version(1, 3)
        .set_required_features_13(features13)
        .set_required_features_12(features12)
        .prefer_gpu_device_type(vkb::PreferredDeviceType::discrete) // I prefer a dGPU over iGPU
        .select();

    if (!physResult) {
        spdlog::error("Failed to select a suitable GPU: {}", physResult.error().message());
        return false;
    }

    vkb::PhysicalDevice vkbPhysicalDevice = physResult.value();
    m_physicalDevice = vkbPhysicalDevice.physical_device;
    spdlog::info("Selected GPU: {}", vkbPhysicalDevice.name);

    // ── Stage 4: Logical Device ────────────────────────────────────────────
    // The VkDevice is my working connection to the selected GPU. It's the
    // handle I pass to vkCreateBuffer, vkCreatePipeline, etc.
    //
    // DeviceBuilder reads the feature requirements I declared in Stage 3 and
    // re-enables them on the device automatically — I don't need to repeat them
    // here. It also enables VK_KHR_swapchain because I provided a surface.
    vkb::DeviceBuilder deviceBuilder(vkbPhysicalDevice);
    auto devResult = deviceBuilder.build();

    if (!devResult) {
        spdlog::error("Failed to create logical device: {}", devResult.error().message());
        return false;
    }

    vkb::Device vkbDevice = devResult.value();
    m_device = vkbDevice.device;

    // ── Stage 5: Graphics Queue ────────────────────────────────────────────
    // A VkQueue is a submission channel on the GPU. I need the graphics queue
    // — the one that accepts draw commands — and its family index.
    //
    // The queue itself isn't created by me (the driver creates queues when the
    // device is created); I just retrieve a handle to it.
    //
    // I store the family index separately because Renderer needs it to create a
    // VkCommandPool (pools are tied to a specific queue family).
    auto queueResult = vkbDevice.get_queue(vkb::QueueType::graphics);
    if (!queueResult) {
        spdlog::error("Failed to get graphics queue: {}", queueResult.error().message());
        return false;
    }
    m_graphicsQueue = queueResult.value();

    auto queueIndexResult = vkbDevice.get_queue_index(vkb::QueueType::graphics);
    if (!queueIndexResult) {
        spdlog::error("Failed to get graphics queue family index: {}", queueIndexResult.error().message());
        return false;
    }
    m_graphicsQueueFamily = queueIndexResult.value();

    // ── Stage 6: Vulkan Memory Allocator ──────────────────────────────────
    // Meshes and textures need buffers/images backed by GPU memory.  Raw
    // Vulkan allocation is deliberately verbose, so I create one VMA allocator
    // here and pass it through the context.
    VmaAllocatorCreateInfo allocatorInfo{};
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    allocatorInfo.physicalDevice = m_physicalDevice;
    allocatorInfo.device = m_device;
    allocatorInfo.instance = m_instance;
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;

    if (vmaCreateAllocator(&allocatorInfo, &m_allocator) != VK_SUCCESS) {
        spdlog::error("Failed to create VMA allocator");
        return false;
    }

    spdlog::info("VulkanContext initialised successfully");
    return true;
}

// =============================================================================
// destroy()
// =============================================================================

/// @details
/// **Why reverse order?**
/// Vulkan is strict about ownership: I must destroy an object before the
/// parent it was created from. Violating this triggers a validation error
/// ("object X destroyed while still in use").
///
/// The reverse of my init() is:
///   Device → Surface → DebugMessenger → Instance
///
/// `VkPhysicalDevice` and `VkQueue` are *not* destroyed here — they are
/// driver-owned references that become invalid automatically when the device
/// and instance they belong to are destroyed.
void VulkanContext::destroy()
{
    // 0. VMA allocator: all VMA buffers/images must be destroyed before this,
    //    and the allocator itself must go before the VkDevice it was built on.
    if (m_allocator != VK_NULL_HANDLE) {
        vmaDestroyAllocator(m_allocator);
        m_allocator = VK_NULL_HANDLE;
    }

    // 1. Logical device: must go first. All VkBuffers, VkImages, VkPipelines,
    //    etc. that I created from this device become invalid after this call.
    if (m_device != VK_NULL_HANDLE) {
        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }

    // 2. Surface: owned by the instance, so it must be destroyed before the
    //    instance. If I destroyed the instance first, this would be a use-
    //    after-free on the instance handle.
    if (m_surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
    }

    // 3. Debug messenger: also instance-owned. I use vk-bootstrap's helper
    //    because the destroy function pointer must be loaded dynamically from
    //    the instance (it's an extension function, not a core Vulkan symbol).
    if (m_debugMessenger != VK_NULL_HANDLE) {
        vkb::destroy_debug_utils_messenger(m_instance, m_debugMessenger);
        m_debugMessenger = VK_NULL_HANDLE;
    }

    // 4. Instance: the very last object to go. Once this is destroyed, no
    //    Vulkan function calls are valid on any object I created from it.
    if (m_instance != VK_NULL_HANDLE) {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }
}
