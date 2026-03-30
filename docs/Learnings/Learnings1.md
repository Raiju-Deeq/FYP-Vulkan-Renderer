# Learnings 1 — Milestone 1 Core Concepts

**Date:** 2026-03-30
**Milestone:** M1 — Baseline Vulkan Pipeline + Triangle
**Goal:** Understand the Vulkan object chain required to render a single triangle

---

## What Happens Before a Triangle Appears

Vulkan is explicit about **everything**. Unlike OpenGL where drivers make hidden decisions, Vulkan requires you to set up a chain of objects manually. Here is the chain for M1, and why each piece exists.

---

## 1. VkInstance — "Hello, Vulkan"

The instance is the application's connection to the Vulkan loader/driver. It declares:
- What Vulkan version we need (1.3)
- What validation layers we want (error checking during development)
- What instance-level extensions we need (debug messenger, surface)

**Why it matters:** Without it, we cannot talk to the GPU at all. Think of it as opening a socket before sending data.

---

## 2. VkSurfaceKHR — "Where will pixels go?"

A surface is the bridge between Vulkan and the windowing system (GLFW/X11/Wayland/Win32). It represents the OS window that will display rendered output.

**Why it matters:** Vulkan does not assume rendering targets a screen. It could render offscreen, to VR headsets, etc. We have to explicitly say "I want to present to *this* window."

---

## 3. VkPhysicalDevice — "Which GPU?"

A machine might have multiple GPUs (integrated Intel + discrete NVIDIA). Physical device selection is where we query what each GPU supports and pick the best one.

For this project we require three **Vulkan 1.3 core features**:

| Feature | Purpose |
|---------|---------|
| `dynamicRendering` | Describe render attachments inline instead of pre-baking VkRenderPass objects. This is the modern approach and the entire architectural foundation of the renderer. |
| `synchronization2` | Cleaner, more explicit API for pipeline barriers and image layout transitions. Replaces the confusing old barrier API. |
| `bufferDeviceAddress` | Lets shaders access buffers via raw GPU pointers. Needed for later milestones (Gaussian splatting). |

**Why it matters:** If we skip feature checking, the app will crash on GPUs that do not support what we need. vk-bootstrap handles this selection for us.

---

## 4. VkDevice — "Open a channel to the GPU"

The logical device is the working connection to the chosen physical GPU. All Vulkan objects (buffers, images, pipelines, command pools) are created from this device.

**Why it matters:** It is the factory for everything. Every Vulkan call after this takes `VkDevice` as a parameter.

---

## 5. VkQueue — "The GPU's inbox"

Queues are how work is submitted to the GPU. The GPU has multiple queue families (graphics, compute, transfer). We need a **graphics queue** — it can do rendering, compute, and transfer.

**Why it matters:** The GPU does not execute commands immediately. Commands are recorded into buffers, then those buffers are submitted to a queue. The GPU processes them asynchronously.

---

## 6. VkSwapchain — "Double-buffered display"

The swapchain is a ring of images that rotate between "being rendered to" and "being displayed on screen." We use 2-3 images (double/triple buffering) so the GPU can render the next frame while the monitor displays the current one.

**Why it matters:** Without a swapchain, rendering to one image would cause the screen to tear or stall waiting.

---

## 7. VkPipeline — "How to draw"

The graphics pipeline is the compiled GPU program: shaders + all the fixed-function state (viewport, rasteriser, blending, vertex input format). In Vulkan, pipelines are **immutable** — they are compiled once upfront.

**Key M1 detail:** Normally pipelines reference a `VkRenderPass` to know the attachment format. With **Dynamic Rendering** (Vulkan 1.3), we instead pass a `VkPipelineRenderingCreateInfo` that says "I will be drawing to a B8G8R8A8_SRGB colour attachment" — no VkRenderPass object needed.

---

## 8. Command Buffers + Synchronisation — "What to draw, and when"

Each frame follows this sequence:

1. **Acquire** a swapchain image (semaphore signals when ready)
2. **Record** commands:
   - Transition image layout (UNDEFINED -> COLOR_ATTACHMENT_OPTIMAL)
   - Begin dynamic rendering (`vkCmdBeginRendering`)
   - Bind pipeline
   - Draw
   - End dynamic rendering (`vkCmdEndRendering`)
   - Transition image layout (COLOR_ATTACHMENT_OPTIMAL -> PRESENT_SRC_KHR)
3. **Submit** the command buffer to the graphics queue
4. **Present** the image to the screen (another semaphore)

Fences throttle the CPU so it does not race ahead of the GPU (double-buffered: `MAX_FRAMES_IN_FLIGHT = 2`).

---

## Image Layout Transitions

Vulkan images have **layouts** that describe how the GPU memory is arranged. Before rendering, the image must be in `COLOR_ATTACHMENT_OPTIMAL`. Before presenting, it must be in `PRESENT_SRC_KHR`.

Transitions between layouts use `VkImageMemoryBarrier2` (from the `synchronization2` feature). This is the "explicit synchronisation" described in the project architecture — no hidden state, no guesswork.

---

## Learning Resources

### Videos
- **Brendan Galea — Vulkan Game Engine Tutorial** (episodes 1-6 cover instance through first triangle) — excellent step-by-step C++ series
- **Travis Vroman — Kohi Game Engine** (Vulkan series) — covers the same ground with more architectural discussion
- **TU Wien — Vulkan Lecture Series** — more academic, good for understanding the "why" behind the API design

### Written Guides
- **vkguide.dev** (Chapter 1: "Initializing Vulkan") — uses vk-bootstrap just like this project, very close to the renderer architecture. **Best starting point.**
- **vulkan-tutorial.com** (chapters up to "Drawing a triangle") — the classic reference, though it uses VkRenderPass (this project adapts to Dynamic Rendering instead)

---

## Key Takeaway

The Vulkan object chain for M1 is:

```
VkInstance -> VkSurfaceKHR -> VkPhysicalDevice -> VkDevice -> VkQueue
                                                     |
                                              VkSwapchainKHR
                                                     |
                                              VkPipeline (Dynamic Rendering)
                                                     |
                                              Command Buffers + Sync
                                                     |
                                                  Triangle!
```

Each object depends on the ones above it. We create them top-to-bottom and destroy them bottom-to-top (RAII reverse order).
