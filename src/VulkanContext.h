/**
* @file VulkanContext.h
 * @brief Core Vulkan bootstrapping: instance, device, queues and validation.
 *
 * VulkanContext owns the fundamental Vulkan objects that must exist before
 * any rendering work can begin:
 *   - VkInstance (with validation layers in debug builds)
 *   - VkDebugUtilsMessengerEXT (validation callback, debug builds only)
 *   - VkPhysicalDevice (GPU selection via vk-bootstrap)
 *   - VkDevice (logical device)
 *   - Graphics and present VkQueues + their family indices
 *
 * All handles are created in init() and destroyed in destroy() in strict
 * reverse order. No other class should own or destroy these handles.
 *
 * @author Mohamed Deeq Mohamed (P2884884)
 * @date   2026-03-29
 */

#ifndef FYP_VULKAN_RENDERER_VULKANCONTEXT_H
#define FYP_VULKAN_RENDERER_VULKANCONTEXT_H

#include <vulkan/vulkan.h>

struct GLFWwindow;

/**
 * @class VulkanContext
 * @brief RAII wrapper around the core Vulkan instance and device.
 *
 * Call init() once at startup with a valid GLFW window.  All returned
 * handles remain valid until destroy() is called or the object goes out
 * of scope.  The class is non-copyable; pass by reference or pointer.
 *
 * Typical usage:
 * @code
 *   VulkanContext ctx;
 *   if (!ctx.init(window)) { ... handle error ... }
 *   // use ctx.device(), ctx.graphicsQueue(), etc.
 *   ctx.destroy();
 * @endcode
 */
class VulkanContext
{
public:
    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------

    /**
     * @brief Initialises the Vulkan instance, selects a physical device,
     *        creates the logical device and retrieves queue handles.
     *
     * Uses vk-bootstrap to select the best available GPU that supports the
     * required Vulkan 1.3 features (dynamicRendering, synchronization2,
     * bufferDeviceAddress).
     *
     * @param  window  Valid GLFW window pointer used for surface creation.
     * @return true    on success.
     * @return false   if any Vulkan initialisation step fails (error is
     *                 printed to stderr via the validation layer callback).
     */
    bool init(GLFWwindow* window);

    /**
     * @brief Destroys all Vulkan handles in the reverse order of creation.
     *
     * Safe to call even if init() was never called or failed partway through.
     */
    void destroy();

    // -------------------------------------------------------------------------
    // Accessors (all return raw handles — caller must not destroy them)
    // -------------------------------------------------------------------------

    /// @brief Returns the Vulkan logical device handle.
    VkDevice         device()              const { return m_device; }

    /// @brief Returns the Vulkan physical device handle.
    VkPhysicalDevice physicalDevice()      const { return m_physicalDevice; }

    /// @brief Returns the Vulkan instance handle.
    VkInstance       instance()            const { return m_instance; }

    /// @brief Returns the window surface handle.
    VkSurfaceKHR     surface()             const { return m_surface; }

    /// @brief Returns the graphics queue handle.
    VkQueue          graphicsQueue()       const { return m_graphicsQueue; }

    /// @brief Returns the queue family index for the graphics queue.
    uint32_t         graphicsQueueFamily() const { return m_graphicsQueueFamily; }

private:
    VkInstance               m_instance             = VK_NULL_HANDLE; ///< Vulkan instance.
    VkPhysicalDevice         m_physicalDevice        = VK_NULL_HANDLE; ///< Selected GPU.
    VkDevice                 m_device                = VK_NULL_HANDLE; ///< Logical device.
    VkSurfaceKHR             m_surface               = VK_NULL_HANDLE; ///< Window surface.
    VkQueue                  m_graphicsQueue         = VK_NULL_HANDLE; ///< Graphics queue.
    uint32_t                 m_graphicsQueueFamily   = 0;              ///< Graphics queue family index.
    VkDebugUtilsMessengerEXT m_debugMessenger        = VK_NULL_HANDLE; ///< Validation layer messenger.
};

#endif // FYP_VULKAN_RENDERER_VULKANCONTEXT_H
