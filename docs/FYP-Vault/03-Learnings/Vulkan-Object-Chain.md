# Learnings — M1 Vulkan Object Chain

**Date:** 2026-03-30
**Milestone:** M1 — Baseline Vulkan Pipeline + Triangle
**Goal:** Understand the Vulkan object chain required to render a single triangle

---

## What Happens Before a Triangle Appears

Vulkan is explicit about **everything**. Unlike OpenGL where drivers make hidden decisions, Vulkan requires you to set up a chain of objects manually.

---

## 1. VkInstance — "Hello, Vulkan"

The instance is the application's connection to the Vulkan loader/driver. It declares:
- What Vulkan version we need (1.3)
- What validation layers we want (error checking during development)
- What instance-level extensions we need (debug messenger, surface)

**Why it matters:** Without it, we cannot talk to the GPU at all.

---

## 2. VkSurfaceKHR — "Where will pixels go?"

A surface is the bridge between Vulkan and the windowing system (GLFW/X11/Wayland/Win32).

**Why it matters:** Vulkan does not assume rendering targets a screen — we have to explicitly say "I want to present to *this* window."

---

## 3. VkPhysicalDevice — "Which GPU?"

For this project we require three **Vulkan 1.3 core features**:

| Feature | Purpose |
|---------|---------|
| `dynamicRendering` | Describe render attachments inline — no VkRenderPass needed |
| `synchronization2` | Cleaner explicit API for pipeline barriers and image layout transitions |
| `bufferDeviceAddress` | Raw GPU pointer access to buffers — needed for M6 Gaussian splatting |

---

## 4. VkDevice — "Open a channel to the GPU"

The logical device is the working connection to the chosen physical GPU. All Vulkan objects are created from this device — it is the factory for everything.

---

## 5. VkQueue — "The GPU's inbox"

Queues are how work is submitted to the GPU. Commands are recorded into buffers, then submitted to a queue. The GPU processes them asynchronously.

---

## 6. VkSwapchain — "Double-buffered display"

The swapchain is a ring of images rotating between "being rendered to" and "being displayed." We use 2 images (`MAX_FRAMES_IN_FLIGHT = 2`) — double buffering.

---

## 7. VkPipeline — "How to draw"

The graphics pipeline is the compiled GPU program: shaders + all fixed-function state. In Vulkan, pipelines are **immutable** — compiled once upfront.

**Key M1 detail:** With **Dynamic Rendering** (Vulkan 1.3), we pass a `VkPipelineRenderingCreateInfo` instead of referencing a `VkRenderPass`.

---

## 8. Command Buffers + Synchronisation

Each frame:
1. **Acquire** a swapchain image (semaphore signals when ready)
2. **Record** commands — transition layout → begin rendering → bind pipeline → draw → end rendering → transition layout
3. **Submit** command buffer to graphics queue
4. **Present** image to screen

Fences throttle the CPU so it does not race ahead of the GPU.

---

## The Full Object Chain

```
VkInstance → VkSurfaceKHR → VkPhysicalDevice → VkDevice → VkQueue
                                                    |
                                             VkSwapchainKHR
                                                    |
                                        VkPipeline (Dynamic Rendering)
                                                    |
                                        Command Buffers + Sync
                                                    |
                                                 Triangle!
```

Create top-to-bottom. Destroy bottom-to-top (RAII).

---

## Learning Resources

- **vkguide.dev** (Chapter 1) — uses vk-bootstrap just like this project. Best starting point.
- **vulkan-tutorial.com** (up to "Drawing a triangle") — classic reference, adapts to Dynamic Rendering
- **Brendan Galea — Vulkan Game Engine Tutorial** (episodes 1-6)
- **TU Wien — Vulkan Lecture Series** — academic perspective on API design

---

*FYP — Vulkan Renderer in C++20 · Mohamed Deeq Mohamed · P2884884 · De Montfort University*
