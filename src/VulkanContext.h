/**
 * @file VulkanContext.h
 * @brief Core Vulkan bootstrapping: instance, physical device, logical device and queues.
 *
 * ## What this file is
 * VulkanContext is the first thing I create at start-up and the last thing
 * I destroy at shutdown. Every other module in my renderer — SwapChain,
 * Pipeline, Renderer, Mesh, Material — depends on the handles stored here.
 *
 * ## Vulkan object overview
 * Vulkan has no global state. I carry everything I need in explicit handles.
 * VulkanContext owns the root four:
 *
 * | Handle              | What it is                                                     |
 * |---------------------|----------------------------------------------------------------|
 * | `VkInstance`        | My application's connection to the Vulkan loader.              |
 * | `VkPhysicalDevice`  | A descriptor for one GPU on the system (not owned, not freed). |
 * | `VkDevice`          | A logical connection to that GPU — the handle I use for work.  |
 * | `VkQueue`           | A channel through which I submit recorded command buffers.     |
 *
 * @note  Physical devices and queues are *retrieved*, not *created*; they are
 *        owned by the driver and must NOT be destroyed explicitly.
 *
 * @author Mohamed Deeq Mohamed (P2884884)
 * @date   2026-03-29
 */

#ifndef FYP_VULKAN_RENDERER_VULKANCONTEXT_H
#define FYP_VULKAN_RENDERER_VULKANCONTEXT_H

#include <vulkan/vulkan.h>

struct GLFWwindow; ///< Forward declaration — avoids including the full GLFW header here.

/**
 * @class VulkanContext
 * @brief RAII wrapper that owns the root Vulkan handles for the lifetime of my application.
 *
 * ## Responsibility
 * VulkanContext handles the one-time bootstrapping sequence that every Vulkan
 * application must perform before any rendering can occur:
 *
 *  1. Create a `VkInstance` — registers my application with the Vulkan loader and
 *     enables validation layers in debug builds.
 *  2. Create a `VkSurfaceKHR` — a platform-neutral handle for the OS window that
 *     I'll present into (created by GLFW so I don't need platform-specific code).
 *  3. Select a `VkPhysicalDevice` — queries all GPUs and picks the best one that
 *     supports Vulkan 1.3, Dynamic Rendering and synchronization2.
 *  4. Create a `VkDevice` — the logical device; from here on this is the handle
 *     I pass to almost every Vulkan function.
 *  5. Retrieve a graphics `VkQueue` and its family index — used to submit draw
 *     work to the GPU and to create command pools later.
 *
 * ## Ownership model
 * I own: `VkInstance`, `VkDevice`, `VkSurfaceKHR`, `VkDebugUtilsMessengerEXT`.
 * I do *not* own `VkPhysicalDevice` or `VkQueue` — those are driver-managed
 * and become invalid when the device/instance they belong to is destroyed.
 *
 * ## Usage
 * @code
 *   VulkanContext ctx;
 *   if (!ctx.init(window)) { return EXIT_FAILURE; }
 *   // ... use ctx.device(), ctx.graphicsQueue(), etc. ...
 *   ctx.destroy();   // must happen before glfwDestroyWindow
 * @endcode
 *
 * @note The class is non-copyable by intent. Pass by reference or pointer.
 */
class VulkanContext
{
public:
    // =========================================================================
    // Lifecycle
    // =========================================================================

    /**
     * @brief Runs the five-stage Vulkan bootstrap sequence.
     *
     * I use vk-bootstrap to avoid hundreds of lines of query/selection
     * boilerplate. The stages are strictly ordered — each one depends on
     * the result of the previous:
     *
     *  - **Instance** before everything else (it's the loader entry point).
     *  - **Surface** before device selection (the selector checks the chosen
     *    GPU can actually present to my window).
     *  - **Physical device** before logical device.
     *  - **Logical device** before queue retrieval.
     *
     * Required GPU features (build will fail if no GPU supports them):
     *  - `dynamicRendering` (Vulkan 1.3) — use vkCmdBeginRendering instead
     *    of pre-baked VkRenderPass objects.
     *  - `synchronization2` (Vulkan 1.3) — cleaner barrier API with
     *    `VkImageMemoryBarrier2` and finer pipeline stage masks.
     *  - `bufferDeviceAddress` (Vulkan 1.2) — lets shaders access buffers
     *    via raw GPU pointers (needed for Gaussian Splatting in M6).
     *
     * @param  window  A valid, already-created GLFW window. GLFW must be
     *                 initialised before calling this function.
     * @return true    All five stages succeeded.
     * @return false   Any stage failed; the error is logged via spdlog.
     *
     * @warning Not idempotent. Calling twice without destroy() in between
     *          will leak Vulkan handles.
     */
    bool init(GLFWwindow* window);

    /**
     * @brief Destroys all owned Vulkan handles in reverse-creation order.
     *
     * Teardown order matters: child objects must be destroyed before the
     * parent they were created from. The sequence mirrors the reverse of init():
     *
     *  1. `VkDevice` (before the instance it was created from).
     *  2. `VkSurfaceKHR` (owned by the instance).
     *  3. `VkDebugUtilsMessengerEXT` (owned by the instance).
     *  4. `VkInstance` (last — everything else depended on it).
     *
     * Every handle is guarded with a `VK_NULL_HANDLE` check, so this is
     * safe to call even if init() failed partway through or was never called.
     */
    void destroy();

    // =========================================================================
    // Accessors
    // =========================================================================
    // All accessors return raw Vulkan handles. The caller must NOT destroy
    // them — I retain ownership for the entire lifetime of this object.

    /**
     * @brief Returns the logical device handle.
     *
     * This is the most-used handle in my renderer. Almost every Vulkan
     * function that creates or destroys an object takes a VkDevice as its
     * first argument.
     */
    VkDevice         device()              const { return m_device; }

    /**
     * @brief Returns the selected physical device (GPU descriptor).
     *
     * Needed by SwapChain to query surface capabilities (supported formats,
     * present modes, image counts) when creating the swapchain.
     */
    VkPhysicalDevice physicalDevice()      const { return m_physicalDevice; }

    /**
     * @brief Returns the Vulkan instance handle.
     *
     * Needed to create the window surface and to tear down the debug
     * messenger. Passed to vk-bootstrap helpers during init().
     */
    VkInstance       instance()            const { return m_instance; }

    /**
     * @brief Returns the window surface handle.
     *
     * The surface is Vulkan's representation of my OS window. SwapChain
     * needs it to create the VkSwapchainKHR that presents rendered images
     * into that window.
     */
    VkSurfaceKHR     surface()             const { return m_surface; }

    /**
     * @brief Returns the graphics queue handle.
     *
     * All my draw command buffers and one-time transfer commands go through
     * this queue. It also doubles as the present queue because vk-bootstrap
     * selects a GPU that supports both graphics and present on the same family.
     */
    VkQueue          graphicsQueue()       const { return m_graphicsQueue; }

    /**
     * @brief Returns the queue family index for the graphics queue.
     *
     * I need this when creating a VkCommandPool — the pool must be told
     * which queue family its command buffers will be submitted to.
     */
    uint32_t         graphicsQueueFamily() const { return m_graphicsQueueFamily; }

private:
    // =========================================================================
    // Owned handles
    // =========================================================================

    /// My application's connection to the Vulkan loader; root of all other objects.
    VkInstance               m_instance             = VK_NULL_HANDLE;

    /// Selected GPU. Not created/owned by me — retrieved from the driver.
    VkPhysicalDevice         m_physicalDevice        = VK_NULL_HANDLE;

    /// Logical device — my primary handle for creating GPU resources.
    VkDevice                 m_device                = VK_NULL_HANDLE;

    /// Platform-neutral window surface; bridge between Vulkan and my GLFW window.
    VkSurfaceKHR             m_surface               = VK_NULL_HANDLE;

    /// Graphics queue — the channel I use to submit command buffers to the GPU.
    VkQueue                  m_graphicsQueue         = VK_NULL_HANDLE;

    /// Index of the graphics queue family (needed when I create command pools).
    uint32_t                 m_graphicsQueueFamily   = 0;

    /// Validation layer debug callback messenger (null in release builds).
    VkDebugUtilsMessengerEXT m_debugMessenger        = VK_NULL_HANDLE;
};

#endif // FYP_VULKAN_RENDERER_VULKANCONTEXT_H
