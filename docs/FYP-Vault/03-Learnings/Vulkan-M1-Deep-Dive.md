---
date: 2026-04-01
tags: [vulkan, fyp, milestone-1, deep-dive]
status: in-progress
---

# Vulkan M1 Deep Dive - From Loader to Triangle

**Milestone:** M1 - Baseline Vulkan Pipeline + Triangle
**Prerequisite:** [[Vulkan-Object-Chain]] (high-level overview)
**Goal:** Understand the internal mechanics of every M1 object deeply enough to write the implementation confidently

---

## 1. VkInstance - What's Actually Happening

When `vkCreateInstance` is called, the **Vulkan loader** (a shared library on the system) wakes up. The loader's job is to find all installed **ICDs** (Installable Client Drivers - the GPU vendor's Vulkan implementation) and all installed **layers** (middleware that sits between the app and the driver).

The key mental model: **Instance = loader + layers + driver connection.**

When vk-bootstrap's `InstanceBuilder` is used, it does three things under the hood:
1. Fills out `VkApplicationInfo` with the app name and API version (1.3)
2. Queries available layers and enables validation if present
3. Queries available instance extensions (surface, debug utils) and enables them

**What to know for coding:** The `vkb::Instance` struct that comes back holds both the raw `VkInstance` handle AND the `VkDebugUtilsMessengerEXT`. Both must be stored - the messenger must be destroyed before the instance.

---

## 2. VkSurfaceKHR - The Platform Bridge

The surface is the one place where Vulkan touches the OS. GLFW abstracts this - `glfwCreateWindowSurface()` calls the right platform-specific function (`vkCreateXlibSurfaceKHR` on Arch Linux, `vkCreateWin32SurfaceKHR` at DMU).

**Critical ordering detail:** The surface must exist *before* physical device selection. Why? Because vk-bootstrap's `PhysicalDeviceSelector` needs the surface to check: "Can this GPU actually present images to this window?" A GPU might support Vulkan 1.3 but not be connected to the monitor.

---

## 3. VkPhysicalDevice - Feature Negotiation

This is where Vulkan differs most from OpenGL. In OpenGL you call `glCreateContext` and hope for the best. In Vulkan, you explicitly **query** what each GPU supports and **reject** any that don't meet your requirements.

The three features required live in two different structs (this trips people up):

```cpp
VkPhysicalDeviceVulkan13Features  // dynamicRendering, synchronization2
VkPhysicalDeviceVulkan12Features  // bufferDeviceAddress
```

Why two structs? Because these features were promoted to core in different Vulkan versions. `bufferDeviceAddress` was originally a Vulkan 1.2 feature. `dynamicRendering` and `synchronization2` were originally 1.3. Even though the project targets 1.3 (which includes everything from 1.2), they are still passed in their respective structs.

**What vk-bootstrap does:** When `set_required_features_13(features13)` is called, it adds those to a checklist. During `select()`, it iterates every GPU on the system, calls `vkGetPhysicalDeviceFeatures2` on each, and rejects any that return `VK_FALSE` for the required features. Then it automatically enables those features when building the logical device.

---

## 4. VkDevice - The Logical vs Physical Distinction

A concept that confuses many beginners: **VkPhysicalDevice is read-only information, VkDevice is the working handle.**

- `VkPhysicalDevice` = "This GPU exists and has these properties" (query it, never modify it)
- `VkDevice` = "I have opened a connection to this GPU with these features enabled" (create things with it)

Every Vulkan object created from here on - buffers, images, pipelines, command pools - takes `VkDevice` as its first parameter. It is the single most-used handle in the codebase, which is why `VulkanContext::device()` is the most-called accessor.

**Destruction rule:** When `vkDestroyDevice` is called, it implicitly invalidates everything created from it. But the Vulkan spec says child objects must be destroyed first - do not rely on implicit cleanup.

---

## 5. VkQueue - The Async Execution Model

This is the biggest mental shift from OpenGL. In OpenGL, calls execute roughly in order - `glDrawArrays` blocks until done (roughly). In Vulkan:

1. **Record** commands into a `VkCommandBuffer` (CPU work - just filling a buffer)
2. **Submit** that buffer to a `VkQueue` (hands it to the GPU)
3. The GPU executes it **asynchronously** - CPU code continues immediately

This means **synchronisation primitives** are needed to coordinate:
- **Semaphores** - GPU-to-GPU sync. "Don't start rendering until the swapchain image is acquired."
- **Fences** - GPU-to-CPU sync. "Don't record new commands until the GPU finished the previous frame."

**Queue families** are groups of queues with the same capabilities. A graphics queue family supports graphics + compute + transfer. Only one queue from the graphics family is needed.

---

## 6. VkSwapchainKHR - The Frame Cycle

The swapchain owns a small array of `VkImage` objects (typically 2-3). Each frame:

```
[Image A: being displayed]  [Image B: being rendered to]
         --- after present ---
[Image A: available]        [Image B: being displayed]
```

`vkAcquireNextImageKHR` gives the index of the next available image. Rendering goes into it, then `vkQueuePresentKHR` sends it to the display. The semaphore mechanism ensures rendering never targets an image still being displayed.

**Present modes** control vsync behaviour:
- `FIFO` - strict vsync (always available, the fallback)
- `MAILBOX` - triple-buffered, lowest latency (the preference if supported)

---

## 7. VkPipeline + Dynamic Rendering - The Modern Approach

A traditional Vulkan pipeline requires a `VkRenderPass` - a large struct describing all attachments, subpasses, and dependencies upfront. Dynamic Rendering (Vulkan 1.3) eliminates this.

At pipeline creation time, provide:
```cpp
VkPipelineRenderingCreateInfo {
    .colorAttachmentCount = 1,
    .pColorAttachmentFormats = &swapchainFormat  // e.g. B8G8R8A8_SRGB
};
```

At draw time, provide:
```cpp
VkRenderingInfo {
    .colorAttachmentCount = 1,
    .pColorAttachments = &colorAttachment  // the actual image view to draw into
};
vkCmdBeginRendering(cmd, &renderingInfo);
// bind pipeline, draw
vkCmdEndRendering(cmd);
```

**Why this is better:** No render pass compatibility rules to manage. No framebuffer objects to recreate on resize. The attachment is described right where it is used. More explicit, which aligns with Vulkan's philosophy.

---

## 8. Image Layout Transitions - The Part Everyone Gets Wrong

This is the concept that causes the most validation errors for beginners. The mental model:

GPU memory for an image can be arranged differently depending on the operation:
- **`UNDEFINED`** - "Don't care what is in here" (initial state, contents are garbage)
- **`COLOR_ATTACHMENT_OPTIMAL`** - "Arranged for fast rendering writes"
- **`PRESENT_SRC_KHR`** - "Arranged for the display controller to read"

Transitions between these require a pipeline barrier. With `synchronization2`, a barrier looks like:

```cpp
VkImageMemoryBarrier2 {
    .srcStageMask  = where the GPU was      // e.g. TOP_OF_PIPE or COLOR_ATTACHMENT_OUTPUT
    .srcAccessMask = what it was doing       // e.g. NONE or COLOR_ATTACHMENT_WRITE
    .dstStageMask  = where the GPU is going  // e.g. COLOR_ATTACHMENT_OUTPUT or BOTTOM_OF_PIPE
    .dstAccessMask = what it will do         // e.g. COLOR_ATTACHMENT_WRITE or NONE
    .oldLayout     = current layout
    .newLayout     = target layout
    .image         = which image
    .subresourceRange = which mip levels and array layers
};
```

The stage/access masks tell the GPU: "Wait until all work at `srcStage` with `srcAccess` is done, then make the memory visible for `dstStage` with `dstAccess`." This prevents data races on the GPU.

---

## Putting It All Together - One Frame

The complete sequence for a single frame in M1:

```
CPU:  Wait on fence[currentFrame]         <- "Is the GPU done with this frame slot?"
CPU:  Acquire swapchain image              <- semaphore signals when image is ready
CPU:  Reset command buffer
CPU:  Begin recording commands:
        Barrier: UNDEFINED -> COLOR_ATTACHMENT_OPTIMAL
        vkCmdBeginRendering (attach the swapchain image view)
        vkCmdBindPipeline (the triangle pipeline)
        vkCmdDraw(3 vertices)
        vkCmdEndRendering
        Barrier: COLOR_ATTACHMENT_OPTIMAL -> PRESENT_SRC_KHR
CPU:  Submit command buffer to graphics queue
        wait on: imageAvailableSemaphore   <- "don't start until image acquired"
        signal: renderFinishedSemaphore    <- "signal when rendering is done"
        signal: fence[currentFrame]        <- "signal when GPU finishes this frame"
CPU:  Present
        wait on: renderFinishedSemaphore   <- "don't present until rendering done"
CPU:  currentFrame = (currentFrame + 1) % 2
```

This double-buffered loop means the CPU can record frame N+1 while the GPU still executes frame N.

---

## Checklist Before Coding

The key things to feel solid on:
1. **Creation/destruction order** - top-down create, bottom-up destroy
2. **Why the surface comes before device selection** - presentation support check
3. **The two feature structs** - Vulkan12Features vs Vulkan13Features
4. **Semaphores vs fences** - GPU-GPU vs GPU-CPU sync
5. **Image layout transitions** - the two barriers per frame and why they exist

---

## Part 2: Going Deeper

The sections above cover the "what" and "why" for each object. The sections below go one level further into the mechanics - the kind of detail that prevents bugs and validation errors when writing real code.

---

### The Vulkan Loader In Detail

When the application launches and calls `vkCreateInstance`, here is what happens step by step on Linux:

1. The dynamic linker loads `libvulkan.so.1` (the Vulkan loader)
2. The loader reads `/usr/share/vulkan/icd.d/*.json` to discover ICDs (e.g. `radeon_icd.x86_64.json` for AMD, `nvidia_icd.json` for NVIDIA)
3. Each ICD JSON file points to a shared library (e.g. `libvulkan_radeon.so`) - the actual GPU driver
4. The loader reads `/usr/share/vulkan/explicit_layer.d/*.json` to discover layers (e.g. `VkLayer_khronos_validation.json`)
5. The loader builds a **dispatch chain**: App -> Layer 1 -> Layer 2 -> ... -> ICD

Every Vulkan function call travels through this chain. The validation layer intercepts calls, checks for errors, and logs them before forwarding to the real driver. This is why validation layers have a performance cost - they are middleware in the call path.

**Why this matters for us:** When running with `VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation`, the environment variable tells the loader to insert the validation layer into the dispatch chain. Without it, calls go directly from app to driver - faster, but no error checking.

The `VkDebugUtilsMessengerEXT` is what routes validation layer output to a callback. vk-bootstrap's `use_default_debug_messenger()` sets up a callback that prints to stderr. Every validation error, warning, or info message comes through this messenger.

---

### Physical Device Selection: What vk-bootstrap Queries

When `PhysicalDeviceSelector::select()` runs, it calls several Vulkan queries on each GPU:

```
vkEnumeratePhysicalDevices          -> list of all GPUs
For each GPU:
  vkGetPhysicalDeviceProperties2    -> name, type (discrete/integrated), limits
  vkGetPhysicalDeviceFeatures2      -> supported features (linked list of feature structs)
  vkGetPhysicalDeviceMemoryProperties -> memory heaps (VRAM size, shared memory)
  vkGetPhysicalDeviceQueueFamilyProperties -> queue families and their capabilities
  vkGetPhysicalDeviceSurfaceSupportKHR -> can this GPU present to our surface?
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR -> min/max image count, extents
  vkGetPhysicalDeviceSurfaceFormatsKHR -> supported swapchain formats
  vkGetPhysicalDeviceSurfacePresentModesKHR -> FIFO, MAILBOX, IMMEDIATE, etc.
```

The selector scores each GPU: discrete GPUs score higher than integrated. GPUs that lack required features are rejected entirely. The surface queries are why the surface must exist first - without it, presentation support cannot be verified.

**The feature chain mechanism:** Vulkan uses a linked list of feature structs via `pNext` pointers. When you pass `VkPhysicalDeviceVulkan13Features` and `VkPhysicalDeviceVulkan12Features`, vk-bootstrap chains them:

```
VkPhysicalDeviceFeatures2
  .pNext -> VkPhysicalDeviceVulkan13Features
              .pNext -> VkPhysicalDeviceVulkan12Features
                          .pNext -> nullptr
```

This chain is passed to `vkGetPhysicalDeviceFeatures2`, which fills in every field. Then vk-bootstrap compares your required features against the filled-in values.

---

### VkDevice Creation: What Gets Enabled

When `DeviceBuilder::build()` runs, it constructs a `VkDeviceCreateInfo` with:

1. **Queue create infos** - which queue families to create queues from, and how many queues per family. We request 1 queue from the graphics family.
2. **Enabled features** - the same feature chain from selection, but this time it tells the driver "I want these features active." Any Vulkan call that requires an enabled feature will crash or produce undefined behaviour if the feature was not enabled at device creation.
3. **Device extensions** - for M1 we need `VK_KHR_swapchain` (to use swapchain functions on the device). vk-bootstrap enables this automatically when a surface is provided.

**A subtlety:** `VK_KHR_swapchain` is a *device* extension, not an instance extension. Swapchain creation functions (`vkCreateSwapchainKHR`, `vkAcquireNextImageKHR`, `vkQueuePresentKHR`) are loaded per-device. The surface functions (`vkCreateXlibSurfaceKHR`, `vkGetPhysicalDeviceSurfaceSupportKHR`) are instance extensions. This is why the surface is created from the instance but the swapchain is created from the device.

---

### Queue Submission: The Full Picture

Queue submission is where the CPU and GPU timelines intersect. Here is the exact mechanism:

```cpp
VkSubmitInfo2 {
    .waitSemaphoreInfoCount = 1,
    .pWaitSemaphoreInfos = &waitInfo,      // wait on imageAvailableSemaphore
    .commandBufferInfoCount = 1,
    .pCommandBufferInfos = &cmdInfo,       // the recorded command buffer
    .signalSemaphoreInfoCount = 1,
    .pSignalSemaphoreInfos = &signalInfo   // signal renderFinishedSemaphore
};
vkQueueSubmit2(graphicsQueue, 1, &submitInfo, inFlightFence);
```

The fence (`inFlightFence`) is signalled when the GPU finishes all commands in this submission. The CPU waits on this fence at the start of the next frame for the same frame slot:

```cpp
vkWaitForFences(device, 1, &fence[currentFrame], VK_TRUE, UINT64_MAX);
vkResetFences(device, 1, &fence[currentFrame]);
```

**Why double buffering works:** With `MAX_FRAMES_IN_FLIGHT = 2`, there are two complete sets of sync objects (semaphores + fence) and two command buffers. While the GPU executes frame 0's commands, the CPU records frame 1's commands into the other command buffer. The fence ensures the CPU never overwrites a command buffer the GPU is still reading.

```
Frame index:   0    1    0    1    0    1
CPU records:  [==] [==] [==] [==] [==] [==]
GPU executes:      [==] [==] [==] [==] [==]
                    ^--- GPU is one frame behind CPU
```

If the CPU is faster than the GPU, the fence blocks it. If the GPU is faster (unlikely with a triangle, but happens with complex scenes), the CPU never waits and the GPU idles briefly between frames.

---

### Semaphores vs Fences: When to Use Which

This is the single most common source of confusion. Here is the rule:

| Primitive | Who waits? | Who signals? | Use case |
|-----------|-----------|-------------|----------|
| **Semaphore** | GPU | GPU | "GPU operation B must wait for GPU operation A" |
| **Fence** | CPU | GPU | "CPU must wait for GPU to finish before proceeding" |

In M1, the synchronisation flow is:

```
vkAcquireNextImageKHR --signals--> imageAvailableSemaphore
                                        |
                                   GPU waits on it
                                        |
                              Command buffer executes
                                        |
                              --signals--> renderFinishedSemaphore
                              --signals--> inFlightFence
                                        |
            vkQueuePresentKHR waits on renderFinishedSemaphore
            Next frame's vkWaitForFences waits on inFlightFence
```

**A common mistake:** Trying to use a semaphore to make the CPU wait. Semaphores have no CPU-side wait function - they only work between GPU queue operations. If the CPU needs to wait for the GPU, it must be a fence.

---

### Image Layout Transitions: The Exact Barriers for M1

Two barriers per frame. Here are the exact values and why each field is set the way it is:

**Barrier 1: Before rendering (UNDEFINED -> COLOR_ATTACHMENT_OPTIMAL)**

```cpp
VkImageMemoryBarrier2 {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
    .srcStageMask  = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
    .srcAccessMask = VK_ACCESS_2_NONE,
    .dstStageMask  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
    .oldLayout     = VK_IMAGE_LAYOUT_UNDEFINED,
    .newLayout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    .image         = swapchainImages[imageIndex],
    .subresourceRange = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0, .levelCount = 1,
        .baseArrayLayer = 0, .layerCount = 1
    }
};
```

- `srcStage = TOP_OF_PIPE` - "There is no previous work to wait for" (we just acquired the image)
- `srcAccess = NONE` - "Nothing was writing to this image before"
- `dstStage = COLOR_ATTACHMENT_OUTPUT` - "The next operation that needs this image is colour output"
- `dstAccess = COLOR_ATTACHMENT_WRITE` - "That operation will write colour data"
- `oldLayout = UNDEFINED` - "We don't care about existing contents" (they are garbage from the last present cycle)
- `newLayout = COLOR_ATTACHMENT_OPTIMAL` - "Rearrange memory for rendering writes"

**Barrier 2: After rendering (COLOR_ATTACHMENT_OPTIMAL -> PRESENT_SRC_KHR)**

```cpp
VkImageMemoryBarrier2 {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
    .srcStageMask  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
    .dstStageMask  = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
    .dstAccessMask = VK_ACCESS_2_NONE,
    .oldLayout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    .newLayout     = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    .image         = swapchainImages[imageIndex],
    .subresourceRange = { /* same as above */ }
};
```

- `srcStage = COLOR_ATTACHMENT_OUTPUT` - "Wait for rendering to finish writing"
- `srcAccess = COLOR_ATTACHMENT_WRITE` - "Specifically, wait for colour writes to complete"
- `dstStage = BOTTOM_OF_PIPE` - "No more GPU pipeline stages after this"
- `dstAccess = NONE` - "The presentation engine reads via a different mechanism (not a pipeline access)"
- `oldLayout = COLOR_ATTACHMENT_OPTIMAL` - "Currently arranged for rendering"
- `newLayout = PRESENT_SRC_KHR` - "Rearrange for the display controller"

Both barriers are submitted via:
```cpp
VkDependencyInfo depInfo {
    .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
    .imageMemoryBarrierCount = 1,
    .pImageMemoryBarriers = &barrier
};
vkCmdPipelineBarrier2(commandBuffer, &depInfo);
```

---

### The Graphics Pipeline State: What M1 Needs

The `VkGraphicsPipelineCreateInfo` for M1 requires these pieces of fixed-function state:

| State | M1 Setting | Why |
|-------|-----------|-----|
| **Vertex input** | Empty (no buffers) | The triangle shader hardcodes positions - no vertex buffer in M1 |
| **Input assembly** | `VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST` | Drawing individual triangles |
| **Viewport + scissor** | Dynamic state | Set per-frame via `vkCmdSetViewport`/`vkCmdSetScissor` so resize works without pipeline recreation |
| **Rasteriser** | `VK_POLYGON_MODE_FILL`, `VK_CULL_MODE_BACK_BIT`, `VK_FRONT_FACE_COUNTER_CLOCKWISE` | Standard fill rasterisation |
| **Multisampling** | `rasterizationSamples = VK_SAMPLE_COUNT_1_BIT` | No MSAA for M1 |
| **Colour blending** | Disabled (write all channels, no blend) | Opaque triangle, no transparency |
| **Dynamic state** | Viewport + Scissor | Avoids pipeline recreation on window resize |
| **Pipeline layout** | Empty (no descriptors, no push constants) | M1 has no uniforms - the shader is fully self-contained |
| **Rendering info** | `VkPipelineRenderingCreateInfo` with 1 colour attachment | Dynamic Rendering - no VkRenderPass |

**Why viewport and scissor are dynamic:** If these were baked into the pipeline, every window resize would require creating a new `VkPipeline`. Dynamic state lets us set them per-frame with `vkCmdSetViewport` / `vkCmdSetScissor` while keeping the same compiled pipeline.

---

### The M1 Shaders: What the GPU Actually Runs

**triangle.vert** - hardcodes three vertices:
```glsl
// Positions and colours are defined as constants in the shader
// gl_VertexIndex (0, 1, or 2) selects which vertex
// No vertex buffer needed - vkCmdDraw(3, 1, 0, 0) triggers 3 invocations
```

The vertex shader outputs `gl_Position` (clip space coordinates) and a `fragColor` varying.

**triangle.frag** - passes the colour through:
```glsl
// Receives interpolated fragColor from the rasteriser
// Writes it to location 0 (the colour attachment)
```

The rasteriser interpolates `fragColor` across the triangle face, giving a smooth colour gradient between the three vertices.

**SPIR-V compilation:** `glslc triangle.vert -o triangle.vert.spv` compiles GLSL to SPIR-V bytecode. The CMake shader target does this automatically. `VkShaderModule` wraps the SPIR-V binary for use in pipeline creation - it is destroyed immediately after the pipeline is compiled (it is not needed at runtime).

---

### What Validation Layers Actually Check

The Khronos validation layer (`VK_LAYER_KHRONOS_validation`) checks for:

1. **Parameter validation** - null handles, invalid enum values, struct sType mismatches
2. **Object lifetime** - using a destroyed handle, double-destroy, leak detection
3. **Synchronisation** - missing barriers, incorrect stage/access masks, layout mismatches
4. **Threading** - calling Vulkan from multiple threads without proper synchronisation
5. **Best practices** - suboptimal usage patterns (enabled via `VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT`)

For M1, the most common validation errors will be:
- Missing image layout transitions (forgetting a barrier)
- Wrong stage/access masks in barriers
- Using an image before its semaphore has signalled
- Submitting a command buffer that was not ended properly

**The goal:** zero validation errors and zero validation warnings on every run. The validation layer is the single most valuable debugging tool in Vulkan development.

---

*FYP - Vulkan Renderer in C++20 · Mohamed Deeq Mohamed · P2884884 · De Montfort University*
