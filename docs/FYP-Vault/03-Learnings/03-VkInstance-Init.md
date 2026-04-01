# Stage 1: Creating VkInstance with vk-bootstrap

> **Vault location:** `docs/FYP-Vault/03-Learnings/03-VkInstance-Init.md`
> **Relates to:** M1 — Baseline Vulkan Pipeline · `VulkanContext::init()` Stage 1 of 5
> **Prerequisites:** Vulkan SDK installed, vk-bootstrap in vcpkg.json, GLFW window created

---

## What You Are Building

`VulkanContext::init(GLFWwindow* window)` is the entry point for all Vulkan initialisation.
It runs exactly once at startup and brings five things into existence in this order:

| Stage | What gets created |
|---|---|
| 1 | `VkInstance` + debug messenger |
| 2 | `VkSurfaceKHR` (GLFW window surface) |
| 3 | `VkPhysicalDevice` (GPU selection) |
| 4 | `VkDevice` + queues |
| 5 | Swapchain |

This note covers **Stage 1 only**: the `VkInstance` and debug messenger.

---

## The Mental Model: What Is a VkInstance?

Before writing any code, understand what you are creating.

The Vulkan API does not have global state. There is no hidden context sitting behind the scenes the way OpenGL has. Everything flows through objects you explicitly create, and `VkInstance` is the root of the entire tree.

Think of it like this: when you call any Vulkan function, the driver needs to know *which application* is making that call, *which extensions* it has opted into, and *which version* of the API it expects. `VkInstance` holds all of that information. It is the contract between your application and the Vulkan loader.

Concretely, creating a `VkInstance` does the following:
- Loads the Vulkan loader library
- Registers your application with the driver
- Activates any **instance-level extensions** you request (e.g. the surface extension for windowing, the debug utils extension for validation messages)
- Activates any **validation layers** you request

Everything else — choosing a GPU, creating a logical device, allocating memory — cannot happen until the instance exists.

---

## What Is the Vulkan Loader?

You will hear this term a lot. The Vulkan loader (`vulkan-1.dll` on Windows, `libvulkan.so` on Linux) is a thin dispatch layer that sits between your application and the actual GPU driver. When you call `vkCreateInstance`, you are not calling directly into NVIDIA or AMD code — you are calling into the loader, which then dispatches to the correct driver.

This design means:
- Multiple GPUs from different vendors can coexist
- Validation layers can be injected transparently (they intercept calls before the driver sees them)
- Your application code is driver-agnostic

---

## What Are Validation Layers?

Vulkan makes almost no assumptions about correctness. If you pass a null pointer where a struct is expected, or transition an image to the wrong layout, the driver is not required to tell you. It may crash, produce garbage, or silently do nothing.

Validation layers are optional diagnostic layers that intercept every Vulkan call and check it for correctness before passing it on to the driver. They catch:
- Incorrect struct usage (missing `sType`, wrong `pNext`)
- Resource lifecycle violations (using a buffer that has been destroyed)
- Synchronisation errors (missing pipeline barriers, incorrect image layouts)
- Parameter validation (null handles, out-of-range values)

They are expensive at runtime, which is why they are disabled in release builds. In debug builds they are **non-negotiable** — validation layers are the single most important tool in Vulkan development. Zero errors on startup and shutdown is your hard requirement.

The primary layer you will use is `VK_LAYER_KHRONOS_validation`, which is the consolidated layer shipped with the Vulkan SDK.

---

## What Is the Debug Messenger?

Even with validation layers enabled, you need a mechanism for the layer to *report* problems to you. That mechanism is `VkDebugUtilsMessengerEXT`.

It is a callback object. You register a function pointer, and whenever the validation layer wants to report something — an error, a warning, a performance note — it calls your function with a message struct containing the severity, type, and message text.

`vkb::InstanceBuilder::use_default_debug_messenger()` registers a built-in callback that prints everything to `stdout` via `fprintf`. For a student project this is exactly what you want. In production you would route messages through your logging system (spdlog in your case — you can wire this up later).

**Important subtlety:** there is a brief window during `vkCreateInstance` and `vkDestroyInstance` itself where the standard debug messenger does not exist yet. vk-bootstrap handles this automatically by also setting up a temporary messenger for those calls via `VkDebugUtilsMessengerCreateInfoEXT` embedded in the instance create info's `pNext` chain.

---

## The vk-bootstrap API

Without vk-bootstrap, creating a `VkInstance` requires:
- Filling `VkApplicationInfo`
- Enumerating and filtering available extensions
- Enumerating and filtering available layers
- Filling `VkInstanceCreateInfo`
- Calling `vkCreateInstance`
- Separately creating the debug messenger
- Error checking every step

vk-bootstrap collapses this into a builder pattern. Here is what each call does:

```cpp
vkb::InstanceBuilder builder;
```
Constructs the builder with safe defaults. No Vulkan calls happen yet.

```cpp
builder.set_app_name("FYP Vulkan Renderer")
```
Populates `VkApplicationInfo::pApplicationName`. The driver can use this for driver-specific optimisations (some drivers have per-application workarounds). It also appears in RenderDoc frame captures, which helps you identify your application.

```cpp
builder.require_api_version(1, 3, 0)
```
Sets `VkApplicationInfo::apiVersion` to `VK_API_VERSION_1_3`. This tells the loader you require Vulkan 1.3 features. If the system's Vulkan implementation is older, instance creation will fail with `VK_ERROR_INCOMPATIBLE_DRIVER`. DMU lab machines and your Arch setup both have Vulkan 1.3-capable drivers, so this is safe.

Why 1.3 specifically? Because Dynamic Rendering (`vkCmdBeginRendering`) was promoted to core in Vulkan 1.3. Without requesting 1.3, you would need to enable it as an extension and check for it manually.

```cpp
builder.request_validation_layers()
```
Requests `VK_LAYER_KHRONOS_validation`. The key word is **request** — this is a graceful fallback. If the Vulkan SDK is not installed on a machine and the layer is absent, instance creation proceeds without it rather than failing. This is the correct behaviour for a cross-platform project: validation on your dev machines, graceful degradation in other environments.

If you want to *require* validation layers and fail hard if absent, you would call `enable_validation_layers()` instead. For your FYP, `request_validation_layers()` is correct.

```cpp
builder.use_default_debug_messenger()
```
Registers a debug callback that prints validation messages to stdout. Internally this enables the `VK_EXT_debug_utils` extension and sets up the `VkDebugUtilsMessengerCreateInfoEXT`.

```cpp
auto build_result = builder.build();
```
This is where the actual Vulkan calls happen. Under the hood vk-bootstrap:
1. Enumerates available layers and extensions
2. Filters your requests against what is available
3. Fills `VkInstanceCreateInfo`
4. Calls `vkCreateInstance`
5. Creates the debug messenger if validation was enabled

The return type is `vkb::Result<vkb::Instance>` — a result wrapper. You must check it before using the value.

---

## Data You Store

After a successful build you extract two things:

```cpp
m_instance      = build_result.value().instance;        // VkInstance
m_debugMessenger = build_result.value().debug_messenger; // VkDebugUtilsMessengerEXT
```

Both must be destroyed explicitly at shutdown in **reverse order** of creation:

```cpp
// In your destructor / cleanup():
vkDestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
vkDestroyInstance(m_instance, nullptr);
```

If you destroy the instance before the messenger, the messenger's handle is invalid and you will get a validation error (or a crash). Reverse-order destruction is a rule you will apply to every Vulkan object you create.

---

## Full Implementation

Here is the complete Stage 1 block as it belongs in `VulkanContext.cpp`.

### VulkanContext.h (relevant members)

```cpp
#pragma once

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

/// @file VulkanContext.h
/// @brief Owns the core Vulkan objects: instance, surface, physical device,
///        logical device, and queues. Initialised once at startup.

class VulkanContext
{
public:
    /// @brief Initialise all Vulkan objects. Must be called before any rendering.
    /// @param window A valid, fully created GLFW window.
    void init(GLFWwindow* window);

    /// @brief Destroy all Vulkan objects in reverse-creation order.
    void cleanup();

private:
    // ── Stage 1 ──────────────────────────────────────────────────────────────
    VkInstance               m_instance       = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;

    // ── Stages 2-5 (added later) ─────────────────────────────────────────────
    VkSurfaceKHR             m_surface        = VK_NULL_HANDLE;
    VkPhysicalDevice         m_physicalDevice = VK_NULL_HANDLE;
    VkDevice                 m_device         = VK_NULL_HANDLE;

    // ── Private stage helpers ────────────────────────────────────────────────
    void createInstance();
};
```

### VulkanContext.cpp — Stage 1

```cpp
#include "VulkanContext.h"

#include <VkBootstrap.h>
#include <spdlog/spdlog.h>

#include <stdexcept>

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

/// @brief Entry point. Calls each stage in order.
void VulkanContext::init(GLFWwindow* window)
{
    createInstance();
    // Stage 2: createSurface(window);   — added next session
    // Stage 3: pickPhysicalDevice();    — added next session
    // Stage 4: createLogicalDevice();   — added next session
    // Stage 5: createSwapchain();       — added next session
}

/// @brief Destroy in strict reverse-creation order.
void VulkanContext::cleanup()
{
    // Stages 5-2 destroyed first (added as they are implemented)

    // Stage 1 — always last
    if (m_debugMessenger != VK_NULL_HANDLE)
    {
        vkDestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
    }
    vkDestroyInstance(m_instance, nullptr);

    spdlog::info("VulkanContext: cleaned up");
}

// ─────────────────────────────────────────────────────────────────────────────
// Stage 1: VkInstance
// ─────────────────────────────────────────────────────────────────────────────

/// @brief Creates the Vulkan instance and debug messenger via vk-bootstrap.
///
/// Uses vkb::InstanceBuilder to:
///   - Target Vulkan 1.3 (required for Dynamic Rendering core promotion)
///   - Request VK_LAYER_KHRONOS_validation (graceful fallback if absent)
///   - Register the default debug messenger (prints to stdout)
///
/// @throws std::runtime_error if instance creation fails.
void VulkanContext::createInstance()
{
    vkb::InstanceBuilder builder;

    auto build_result = builder
        .set_app_name("FYP Vulkan Renderer")
        .require_api_version(1, 3, 0)
        .request_validation_layers()      // graceful: skips if SDK not present
        .use_default_debug_messenger()    // prints validation output to stdout
        .build();

    if (!build_result)
    {
        throw std::runtime_error(
            "vkb::InstanceBuilder failed: " + build_result.error().message()
        );
    }

    vkb::Instance vkb_instance = build_result.value();

    m_instance       = vkb_instance.instance;
    m_debugMessenger = vkb_instance.debug_messenger;

    spdlog::info("VulkanContext: VkInstance created (Vulkan 1.3)");
    spdlog::info("VulkanContext: debug messenger active");
}
```

---

## What Validation Output Looks Like

When Stage 1 works correctly and validation layers are active, you will see nothing (silence is success — the messenger only fires when something is wrong).

If you make an error — for example, destroying the instance before the messenger — you will see output like:

```
VUID-vkDestroyInstance-instance-00629(ERROR / SPEC): 
  Object: 0x... (Type = VkDebugUtilsMessengerEXT)
  |  OBJ ERROR : For VkInstance 0x..., VkDebugUtilsMessengerEXT 0x... has not been destroyed.
```

This is exactly the information you need to fix the problem. The VUID (Vulkan Usage ID) is a unique identifier you can look up in the Vulkan spec for full context.

---

## Validation Checklist for Stage 1

Before moving to Stage 2, confirm all of these:

- [ ] Application starts without crashing
- [ ] No validation errors on startup (silence = clean)
- [ ] No validation errors on shutdown (check after `cleanup()` returns)
- [ ] spdlog prints "VkInstance created (Vulkan 1.3)" and "debug messenger active"
- [ ] RenderDoc can attach to the process (this confirms the instance is well-formed)

---

## Common Mistakes at This Stage

**Forgetting to check `build_result`.**
vk-bootstrap returns a result type, not a raw value. If you call `.value()` on a failed result, you get undefined behaviour. Always check `if (!build_result)` first.

**Not storing `m_debugMessenger` separately.**
`vkb::Instance` is a stack object. Once `createInstance()` returns, the `vkb_instance` local goes out of scope. You must extract `.instance` and `.debug_messenger` into your member variables before the function returns.

**Destroying the instance before the messenger in cleanup.**
The messenger is owned by the instance. Destroy the messenger first, then the instance.

**Calling `vkDestroyDebugUtilsMessengerEXT` with a null handle.**
If validation layers were not available, `m_debugMessenger` will be `VK_NULL_HANDLE`. The guard `if (m_debugMessenger != VK_NULL_HANDLE)` in the cleanup function above prevents this. Always guard optional-object destruction.

---

## Further Reading

- **Vulkan Spec — VkInstanceCreateInfo:** https://registry.khronos.org/vulkan/specs/1.3/html/vkspec.html#VkInstanceCreateInfo
- **vk-bootstrap source (InstanceBuilder):** https://github.com/charles-lunarg/vk-bootstrap/blob/main/src/VkBootstrap.h
- **Vulkan Guide — Initialisation:** https://vkguide.dev/docs/new_chapter_1/vulkan_init_code/
- **Vulkan Tutorial — Instance:** https://vulkan-tutorial.com/Drawing_a_triangle/Setup/Instance
- **YouTube — Brendan Galea "Vulkan Game Engine" Ep.1** (manual instance creation — useful contrast to understand what vk-bootstrap hides): https://www.youtube.com/watch?v=Y9U9IE0gVHA
- **YouTube — vkguide.dev stream VODs** — search "vkguide new vulkan" on YouTube for the Dynamic Rendering series

---

*Next note: Stage 2 — VkSurfaceKHR via `glfwCreateWindowSurface`*
