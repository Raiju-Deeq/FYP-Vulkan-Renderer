# Week 1 Day 3 Plan — Implement VulkanContext (Milestone 1, Step 1)

**Date:** 2026-03-30
**Milestone:** M1 — Baseline Vulkan Pipeline
**Target:** Implement VulkanContext.cpp (instance, device, queues via vk-bootstrap)

---

## Context

All source files are scaffolded stubs from Week 1. VulkanContext is the foundational module — SwapChain, Pipeline, and Renderer all depend on it. We need to implement the Vulkan instance, device, and queue setup using vk-bootstrap before anything else can work.

---

## Files to Modify

1. **`src/VulkanContext.cpp`** — implement `init()` and `destroy()` (main work)
2. **`CMakeLists.txt`** — fix duplicate source file entries (lines 10-32)

**Read-only references:**
- `src/VulkanContext.h` — class interface (already complete, no changes needed)

---

## Implementation: `VulkanContext.cpp`

### `init(GLFWwindow* window)` — 5 stages

**Stage 1: Create VkInstance** via `vkb::InstanceBuilder`
- App name: "FYP Vulkan Renderer"
- `require_api_version(1, 3, 0)`
- `request_validation_layers()` (graceful fallback if not present)
- `use_default_debug_messenger()` (prints validation messages to stdout)
- Store `m_instance` and `m_debugMessenger`

**Stage 2: Create window surface** via `glfwCreateWindowSurface()`
- Must happen before device selection (vk-bootstrap needs surface for present support check)

**Stage 3: Select physical device** via `vkb::PhysicalDeviceSelector`
- Pass vkbInstance + m_surface to constructor
- `set_minimum_version(1, 3)`
- Require Vulkan 1.3 features: `dynamicRendering`, `synchronization2` (via `set_required_features_13()`)
- Require Vulkan 1.2 features: `bufferDeviceAddress` (via `set_required_features_12()`)
- `prefer_gpu_device_type(vkb::PreferredDeviceType::discrete)`
- Log selected GPU name

**Stage 4: Create logical device** via `vkb::DeviceBuilder`
- Features already propagated from physical device selection
- Store `m_device`

**Stage 5: Retrieve graphics queue + family index**
- `vkbDevice.get_queue(vkb::QueueType::graphics)` → `m_graphicsQueue`
- `vkbDevice.get_queue_index(vkb::QueueType::graphics)` → `m_graphicsQueueFamily`

**Error handling:** Each stage checks the vk-bootstrap Result, logs via `spdlog::error()`, returns `false`. Caller can safely call `destroy()` after any failure since all handles default to VK_NULL_HANDLE.

### `destroy()` — reverse teardown

1. `vkDestroyDevice(m_device)`
2. `vkDestroySurfaceKHR(m_instance, m_surface)`
3. `vkb::destroy_debug_utils_messenger(m_instance, m_debugMessenger)`
4. `vkDestroyInstance(m_instance)`

Each guarded by `!= VK_NULL_HANDLE`, reset after destruction for idempotency.

### Includes needed
```cpp
#include <VkBootstrap.h>
#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>
```

---

## Fix: CMakeLists.txt duplicate sources

Lines 10-32 listed source files twice. Fixed to a single clean list of `.cpp` files only:

```cmake
set(SOURCES
    src/main.cpp
    src/VulkanContext.cpp
    src/SwapChain.cpp
    src/Pipeline.cpp
    src/Renderer.cpp
    src/Mesh.cpp
    src/Material.cpp
    src/GaussianSplat.cpp
)
```

---

## Verification

1. `cmake --preset linux-debug`
2. `cmake --build --preset linux-debug` — zero errors
3. Full integration test comes when we wire main.cpp + SwapChain

---

## Next Steps

| Step | Module | Description |
|------|--------|-------------|
| 2 | SwapChain | Swapchain + image views via vk-bootstrap |
| 3 | Pipeline | SPIR-V loading + graphics pipeline (Dynamic Rendering) |
| 4 | Renderer | Command recording + present loop |
| 5 | main.cpp | Wire everything together with GLFW window + main loop |
| 6 | Shaders | triangle.vert / triangle.frag |
