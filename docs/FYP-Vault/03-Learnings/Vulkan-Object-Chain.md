# Vulkan Object Chain — M1 Core Concepts

**Date:** 2026-03-30
**Milestone:** M1 — Baseline Vulkan Pipeline + Triangle
**Goal:** Understand every object required to render a single triangle, and *why* each one exists

---

## What Happens Before a Triangle Appears

Vulkan is explicit about **everything**. Unlike OpenGL where drivers make hidden decisions, Vulkan requires you to set up a chain of objects manually. Here is the chain for M1, and why each piece exists.

---

## 1. VkInstance — “Hello, Vulkan”

The instance is the application’s connection to the Vulkan loader/driver. It declares:
- What Vulkan version we need (1.3)
- What validation layers we want (error checking during development)
- What instance-level extensions we need (debug messenger, surface)

**Why it matters:** Without it, we cannot talk to the GPU at all. Think of it as opening a socket before sending any data — nothing else is possible until this exists.

---

## 2. VkSurfaceKHR — “Where will pixels go?”

A surface is the bridge between Vulkan and the windowing system (GLFW / X11 / Wayland / Win32). It represents the OS window that will receive rendered output.

**Why it matters:** Vulkan does not assume rendering targets a screen at all — it could render offscreen, to VR headsets, or into compute pipelines. We have to explicitly say “I want to present to *this* window.”

---

## 3. VkPhysicalDevice — “Which GPU?”

A machine may have multiple GPUs (integrated Intel + discrete NVIDIA). Physical device selection queries what each GPU supports and picks the best match. If we skip feature checking the app will crash on unsupported hardware — vk-bootstrap handles this selection.

This project requires three **Vulkan 1.3 core features**:

| Feature | Purpose |
|---------|---------|
| `dynamicRendering` | Describe render attachments inline at command recording time — no `VkRenderPass` objects needed. The architectural foundation of this renderer. |
| `synchronization2` | Cleaner, more explicit API for pipeline barriers and image layout transitions. Replaces the confusing old barrier API entirely. |
| `bufferDeviceAddress` | Lets shaders access buffers via raw GPU pointers. Required for M6 Gaussian Splatting splat buffers. |

---

## 4. VkDevice — “Open a channel to the GPU”

The logical device is the working connection to the chosen physical GPU. All Vulkan objects — buffers, images, pipelines, command pools — are created through this device. Every Vulkan call after this point takes `VkDevice` as a parameter. It is the factory for everything.

---

## 5. VkQueue — “The GPU’s inbox”

Queues are how work reaches the GPU. The GPU exposes multiple queue families (graphics, compute, transfer). We need a **graphics queue** — it can handle rendering, compute, and transfer. Commands are recorded into command buffers and then submitted to a queue; the GPU processes them asynchronously.

---

## 6. VkSwapchainKHR — “Double-buffered display”

The swapchain is a ring of images rotating between “being rendered to” and “being displayed on screen.” This project uses 2 images (`MAX_FRAMES_IN_FLIGHT = 2`) — double buffering. Without it, rendering to a single image would cause tearing or stalling while the monitor finishes displaying the previous frame.

---

## 7. VkPipeline — “How to draw”

The graphics pipeline is the compiled GPU program: vertex and fragment shaders plus all fixed-function state (viewport, rasteriser, depth test, blending, vertex input format). Pipelines in Vulkan are **immutable** — compiled once upfront, never modified.

**Key M1 detail:** Normally a pipeline must reference a `VkRenderPass` to know the attachment format. With **Dynamic Rendering** (Vulkan 1.3), we instead provide a `VkPipelineRenderingCreateInfo` that describes the attachment format inline — no `VkRenderPass` object is ever created.

---

## 8. Command Buffers + Synchronisation — “What to draw, and when”

Each frame follows this sequence:

1. **Acquire** a swapchain image (semaphore signals when the image is ready)
2. **Record** commands into a command buffer:
   - Transition image layout `UNDEFINED → COLOR_ATTACHMENT_OPTIMAL` (barrier)
   - `vkCmdBeginRendering` — begin dynamic rendering pass
   - Bind pipeline
   - Draw
   - `vkCmdEndRendering`
   - Transition image layout `COLOR_ATTACHMENT_OPTIMAL → PRESENT_SRC_KHR` (barrier)
3. **Submit** the command buffer to the graphics queue
4. **Present** the image to the screen

Fences throttle the CPU so it cannot submit more than `MAX_FRAMES_IN_FLIGHT` frames ahead of the GPU.

---

## Image Layout Transitions

Vulkan images have **layouts** that describe how GPU memory is physically arranged for a given operation. Two transitions are required every frame:

| From | To | When |
|------|----|------|
| `UNDEFINED` | `COLOR_ATTACHMENT_OPTIMAL` | Before rendering into the image |
| `COLOR_ATTACHMENT_OPTIMAL` | `PRESENT_SRC_KHR` | Before handing the image to the swapchain for display |

Transitions are performed via `VkImageMemoryBarrier2` (synchronization2). This is what “explicit synchronisation” means in the architecture — no hidden state, no driver guesswork. Source and destination stage/access masks must be specified explicitly for every transition.

---

## Full Object Chain

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

Create top-to-bottom. Destroy bottom-to-top (RAII reverse order).

---

## Learning Resources

### Written
- **[vkguide.dev](https://vkguide.dev/)** — Chapter 1: “Initialising Vulkan”. Uses vk-bootstrap directly, architecturally closest to this project. **Best starting point.**
- **[vulkan-tutorial.com](https://vulkan-tutorial.com/)** — Classic reference up to “Drawing a triangle”. Uses `VkRenderPass`; adapt to Dynamic Rendering.

### Video
- **Brendan Galea — Vulkan Game Engine Tutorial** (episodes 1–6) — step-by-step C++ series covering instance through first triangle
- **Travis Vroman — Kohi Game Engine** (Vulkan series) — same ground with more architectural discussion
- **TU Wien — Vulkan Lecture Series** — academic perspective on the API design rationale

---

*FYP — Vulkan Renderer in C++20 · Mohamed Deeq Mohamed · P2884884 · De Montfort University*
