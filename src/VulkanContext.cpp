/**
 * @file VulkanContext.cpp
 * @brief Implementation of VulkanContext — Vulkan instance, device and queue setup.
 *
 * This is the foundational module of the renderer. Every other module (SwapChain,
 * Pipeline, Renderer, Mesh, Material) depends on the handles created here.
 *
 * The init() function follows a strict 5-stage sequence using vk-bootstrap to
 * handle the boilerplate of Vulkan initialisation. Each stage depends on the
 * one before it — the order cannot be changed.
 *
 * The destroy() function tears everything down in the exact reverse order,
 * which is a hard requirement of the Vulkan spec (child objects must be
 * destroyed before their parents).
 *
 * @author Mohamed Deeq Mohamed (P2884884)
 * @date   2026-04-02
 *
 * @see VulkanContext.h for the class interface and member documentation.
 * @see https://github.com/charles-lunarg/vk-bootstrap for the vk-bootstrap API.
 */

#include "VulkanContext.h"

#include <VkBootstrap.h>   // vk-bootstrap — handles instance, device and queue creation
#include <GLFW/glfw3.h>    // GLFW — only used here for glfwCreateWindowSurface()
#include <spdlog/spdlog.h> // spdlog — structured logging (replaces std::cout)

/// @details
/// Initialisation follows 5 sequential stages, each building on the last:
///   1. Create VkInstance via vkb::InstanceBuilder
///   2. Create VkSurfaceKHR via GLFW (needed before GPU selection)
///   3. Select VkPhysicalDevice via vkb::PhysicalDeviceSelector
///   4. Create VkDevice via vkb::DeviceBuilder
///   5. Retrieve VkQueue + queue family index
///
/// If any stage fails, the function logs the error and returns false.
/// All member handles default to VK_NULL_HANDLE, so destroy() is always
/// safe to call regardless of where the failure occurred.
bool VulkanContext::init(GLFWwindow* window)
{
    // ── Stage 1: Instance ──────────────────────────────────────────────
    // The VkInstance is my application's connection to the Vulkan loader.
    // vk-bootstrap's InstanceBuilder collects my requirements (API version,
    // validation layers, debug messenger) and calls vkCreateInstance for me.
    vkb::InstanceBuilder builder;
    auto instResult = builder
        .set_app_name("FYP Vulkan Renderer")
        .require_api_version(1, 3, 0)
        .request_validation_layers()
        .use_default_debug_messenger()
        .build();

    // vk-bootstrap returns a Result type — if it converted to false,
    // something went wrong (e.g. Vulkan 1.3 not supported on this machine).
    if (!instResult) {
        spdlog::error("Failed to create Vulkan instance: {}", instResult.error().message());
        return false;
    }

    // I store the full vkb::Instance as a local because Stage 3's
    // PhysicalDeviceSelector needs the vkb wrapper, not just the raw handle.
    vkb::Instance vkbInstance = instResult.value();
    m_instance      = vkbInstance.instance;
    m_debugMessenger = vkbInstance.debug_messenger;

    // ── Stage 2: Surface ───────────────────────────────────────────────
    // The surface is the bridge between Vulkan and my GLFW window. Under
    // the hood this calls the platform-specific function (vkCreateXlibSurfaceKHR
    // on my Arch machine, vkCreateWin32SurfaceKHR on the DMU lab PCs).
    //
    // This MUST happen before physical device selection — vk-bootstrap needs
    // the surface to verify the chosen GPU can actually present to my window.
    if (glfwCreateWindowSurface(m_instance, window, nullptr, &m_surface) != VK_SUCCESS) {
        spdlog::error("Failed to create window surface");
        return false;
    }

    // ── Stage 3: Physical Device ───────────────────────────────────────
    // Here I tell vk-bootstrap which GPU features my renderer needs.
    // It queries every GPU on the system and rejects any that don't meet
    // these requirements. The features live in two separate structs because
    // they were promoted to core in different Vulkan versions (1.2 and 1.3).

    // Vulkan 1.3 features — these are the core of my renderer's architecture:
    //   dynamicRendering: lets me describe render attachments inline instead
    //                     of pre-baking VkRenderPass objects (the modern approach)
    //   synchronization2: cleaner barrier API for image layout transitions
    VkPhysicalDeviceVulkan13Features features13{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES
    };
    features13.dynamicRendering = VK_TRUE;
    features13.synchronization2 = VK_TRUE;

    // Vulkan 1.2 features:
    //   bufferDeviceAddress: lets shaders access buffers via raw GPU pointers.
    //                        I'll need this for the Gaussian Splatting milestone (M6).
    VkPhysicalDeviceVulkan12Features features12{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES
    };
    features12.bufferDeviceAddress = VK_TRUE;

    // The selector takes the vkb::Instance AND my surface so it can check
    // both feature support and presentation support for each GPU.
    vkb::PhysicalDeviceSelector selector(vkbInstance, m_surface);
    auto physResult = selector
        .set_minimum_version(1, 3)
        .set_required_features_13(features13)
        .set_required_features_12(features12)
        .prefer_gpu_device_type(vkb::PreferredDeviceType::discrete)
        .select();

    if (!physResult) {
        spdlog::error("Failed to select a suitable GPU: {}", physResult.error().message());
        return false;
    }

    // I store the vkb::PhysicalDevice locally because Stage 4's
    // DeviceBuilder needs the full wrapper to read the required features.
    vkb::PhysicalDevice vkbPhysicalDevice = physResult.value();
    m_physicalDevice = vkbPhysicalDevice.physical_device;
    spdlog::info("Selected GPU: {}", vkbPhysicalDevice.name);

    // ── Stage 4: Logical Device ────────────────────────────────────────
    // The logical device is my working connection to the GPU. Every Vulkan
    // object I create from here on (buffers, pipelines, command pools) goes
    // through this device handle.
    //
    // DeviceBuilder reads the features I required in Stage 3 and
    // automatically enables them — I don't need to re-specify anything.
    // It also enables VK_KHR_swapchain since I provided a surface.
    vkb::DeviceBuilder deviceBuilder(vkbPhysicalDevice);
    auto devResult = deviceBuilder.build();

    if (!devResult) {
        spdlog::error("Failed to create logical device: {}", devResult.error().message());
        return false;
    }

    vkb::Device vkbDevice = devResult.value();
    m_device = vkbDevice.device;

    // ── Stage 5: Graphics Queue ────────────────────────────────────────
    // Queues are how I submit work to the GPU. I need a graphics queue
    // (for rendering) and its family index (for creating a command pool
    // in Renderer later). The GPU processes submitted commands asynchronously.
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

    spdlog::info("VulkanContext initialised successfully");
    return true;
}

/// @details
/// Teardown follows the exact reverse of the creation order in init().
/// Each handle is guarded with a VK_NULL_HANDLE check so this function
/// is safe to call even if init() failed partway through or was never called.
///
/// VkPhysicalDevice and VkQueue are NOT destroyed here — they are not owned
/// handles. They are just references retrieved from the driver and become
/// invalid automatically when the instance/device they belong to is destroyed.
void VulkanContext::destroy()
{
    // 1. Logical device — must go first, before the instance it was created from.
    if (m_device != VK_NULL_HANDLE) {
        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }

    // 2. Surface — owned by the instance, must be destroyed before the instance.
    if (m_surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
    }

    // 3. Debug messenger — also owned by the instance. I use vk-bootstrap's
    //    helper instead of manually loading the destroy function pointer.
    if (m_debugMessenger != VK_NULL_HANDLE) {
        vkb::destroy_debug_utils_messenger(m_instance, m_debugMessenger);
        m_debugMessenger = VK_NULL_HANDLE;
    }

    // 4. Instance — last to go, everything above depended on it.
    if (m_instance != VK_NULL_HANDLE) {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }
}
