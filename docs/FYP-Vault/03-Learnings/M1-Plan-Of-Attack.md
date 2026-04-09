---
date: 2026-04-09
tags: [vulkan, fyp, milestone-1, planning, dynamic-rendering]
status: in-progress
---

# M1 Plan of Attack — Dynamic Rendering Triangle

> **Goal:** Render a colour-interpolated triangle to a GLFW window using Vulkan 1.3 Dynamic Rendering.  
> **Deadline:** Week 2 — 13 Apr 2026  
> **Shaders ready:** `triangle.vert.spv` + `triangle.frag.spv` (already compiled)  
> **VulkanContext:** Complete — device, queue, surface all initialised.

---

## What Needs to Happen

Four modules are currently stubs. They must be implemented **in dependency order**:

```
SwapChain → Pipeline → Renderer → main.cpp
```

No step can be skipped. The Renderer needs the swapchain image count and format; the Pipeline
needs the swapchain colour format; main.cpp wires everything together.

---

## Step 1 — SwapChain::init() and SwapChain::destroy()

**File:** `src/SwapChain.cpp`

### 1a. Create the swapchain

Use `vkb::SwapchainBuilder` from vk-bootstrap. Pass the `VkPhysicalDevice`, `VkDevice`, and `VkSurfaceKHR` from VulkanContext.

```cpp
vkb::SwapchainBuilder builder{ ctx.physicalDevice(), ctx.device(), ctx.surface() };
auto result = builder
    .set_desired_format({ VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
    .set_desired_present_mode(VK_PRESENT_MODE_MAILBOX_KHR)
    .set_fallback_present_mode(VK_PRESENT_MODE_FIFO_KHR)
    .set_desired_extent(width, height)
    .build();
```

Store: `m_swapchain`, `m_format`, `m_extent`.

### 1b. Retrieve swapchain images

vk-bootstrap gives these for free:

```cpp
m_images     = vkbSwapchain.get_images().value();
m_imageViews = vkbSwapchain.get_image_views().value();
```

### 1c. destroy()

Destroy image views first, then the swapchain:

```cpp
for (auto iv : m_imageViews)
    vkDestroyImageView(device, iv, nullptr);
vkDestroySwapchainKHR(device, m_swapchain, nullptr);
```

### 1d. acquireNextImage()

Thin wrapper around `vkAcquireNextImageKHR`. Returns `VK_ERROR_OUT_OF_DATE_KHR` on resize — the
caller (Renderer) is responsible for rebuilding.

```cpp
VkResult SwapChain::acquireNextImage(VkDevice device, VkSemaphore signal, uint32_t& outIndex) {
    return vkAcquireNextImageKHR(device, m_swapchain,
                                  UINT64_MAX, signal, VK_NULL_HANDLE, &outIndex);
}
```

**Validation check:** Zero image views in the vector means something went wrong. Assert or log immediately.

---

## Step 2 — Pipeline::init()

**File:** `src/Pipeline.cpp`

### 2a. loadSpv() (static helper)

Read a `.spv` file as raw bytes into a `std::vector<uint32_t>`:

```cpp
std::ifstream file(path, std::ios::binary | std::ios::ate);
size_t fileSize = file.tellg();
std::vector<uint32_t> code(fileSize / 4);
file.seekg(0);
file.read(reinterpret_cast<char*>(code.data()), fileSize);
return code;
```

Check `fileSize % 4 == 0` — SPIR-V is always 4-byte aligned.

### 2b. createShaderModule() (static helper)

```cpp
VkShaderModuleCreateInfo info{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
info.codeSize = code.size() * 4;
info.pCode    = code.data();
VkShaderModule mod;
vkCreateShaderModule(device, &info, nullptr, &mod);
return mod;
```

### 2c. Empty pipeline layout

M1 has no descriptors or push constants:

```cpp
VkPipelineLayoutCreateInfo layoutInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
// setLayoutCount = 0, pushConstantRangeCount = 0
vkCreatePipelineLayout(device, &layoutInfo, nullptr, &m_layout);
```

### 2d. Assemble VkGraphicsPipelineCreateInfo

Key states for M1:

| State | Setting |
|-------|---------|
| Vertex input | Empty — vertices are hardcoded in the shader |
| Input assembly | `VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST` |
| Viewport/scissor | Dynamic state (set per-frame) |
| Rasteriser | Fill, back-face cull, CCW front face |
| Multisampling | 1× (no MSAA for M1) |
| Depth/stencil | None (no depth attachment for M1) |
| Colour blend | No blending — write-all mask |
| Dynamic state | `VK_DYNAMIC_STATE_VIEWPORT` + `VK_DYNAMIC_STATE_SCISSOR` |

### 2e. Dynamic Rendering attachment (CRITICAL)

**No VkRenderPass.** Instead, chain a `VkPipelineRenderingCreateInfoKHR` into `pNext`:

```cpp
VkPipelineRenderingCreateInfoKHR renderInfo{
    VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR
};
renderInfo.colorAttachmentCount    = 1;
renderInfo.pColorAttachmentFormats = &colourFormat; // from swapchain
// graphicsPipelineInfo.pNext = &renderInfo
```

**This is what replaces `renderPass` — do not pass a renderPass handle.**

### 2f. Destroy shader modules immediately after pipeline creation

```cpp
vkDestroyShaderModule(device, vertModule, nullptr);
vkDestroyShaderModule(device, fragModule, nullptr);
```

They are no longer needed once the pipeline is compiled.

---

## Step 3 — Renderer::init(), Renderer::drawFrame(), Renderer::destroy()

**File:** `src/Renderer.cpp`

### 3a. Command pool

```cpp
VkCommandPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
poolInfo.queueFamilyIndex = ctx.graphicsQueueFamily();
vkCreateCommandPool(device, &poolInfo, nullptr, &m_commandPool);
```

### 3b. Allocate command buffers

```cpp
VkCommandBufferAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
allocInfo.commandPool        = m_commandPool;
allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
allocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
vkAllocateCommandBuffers(device, &allocInfo, m_commandBuffers);
```

### 3c. Semaphores and fences

For each frame `i` in `[0, MAX_FRAMES_IN_FLIGHT)`:

```cpp
VkSemaphoreCreateInfo semInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
vkCreateSemaphore(device, &semInfo, nullptr, &m_imageAvailableSemaphores[i]);
vkCreateSemaphore(device, &semInfo, nullptr, &m_renderFinishedSemaphores[i]);

VkFenceCreateInfo fenceInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Start signalled so frame 0 doesn't hang
vkCreateFence(device, &fenceInfo, nullptr, &m_inFlightFences[i]);
```

> **Why start signalled?** On the very first frame, `vkWaitForFences` is called before any GPU
> work has been submitted. If the fence starts unsignalled, the CPU stalls forever.

### 3d. drawFrame() — the full per-frame sequence

```
1. vkWaitForFences(device, 1, &fence[i], VK_TRUE, UINT64_MAX)
2. vkResetFences(device, 1, &fence[i])
3. swapchain.acquireNextImage(device, imageAvailable[i], &imageIndex)
4. vkResetCommandBuffer(cmd[i], 0)
5. Begin command buffer recording
6. Barrier: UNDEFINED → COLOR_ATTACHMENT_OPTIMAL
7. vkCmdBeginRendering (Dynamic Rendering)
8. vkCmdSetViewport + vkCmdSetScissor
9. vkCmdBindPipeline
10. vkCmdDraw(3, 1, 0, 0)  ← 3 hardcoded vertices, 1 instance
11. vkCmdEndRendering
12. Barrier: COLOR_ATTACHMENT_OPTIMAL → PRESENT_SRC_KHR
13. End command buffer recording
14. vkQueueSubmit (wait imageAvailable, signal renderFinished, signal fence)
15. vkQueuePresentKHR (wait renderFinished)
16. m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT
```

### 3e. Image layout barriers (synchronization2)

Use `VkImageMemoryBarrier2` — Vulkan 1.3 core.

**Barrier 1: Transition to render target**

```cpp
VkImageMemoryBarrier2 barrier{};
barrier.sType         = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
barrier.srcStageMask  = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
barrier.srcAccessMask = VK_ACCESS_2_NONE;
barrier.dstStageMask  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
barrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
barrier.oldLayout     = VK_IMAGE_LAYOUT_UNDEFINED;
barrier.newLayout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
barrier.image         = swapchainImage;
barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
```

**Barrier 2: Transition to present**

```cpp
barrier.srcStageMask  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
barrier.dstStageMask  = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
barrier.dstAccessMask = VK_ACCESS_2_NONE;
barrier.oldLayout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
barrier.newLayout     = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
```

Wrap both in `VkDependencyInfo` and call `vkCmdPipelineBarrier2`.

### 3f. vkCmdBeginRendering

```cpp
VkRenderingAttachmentInfoKHR colourAttach{
    VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR
};
colourAttach.imageView   = swapchain.imageViews()[imageIndex];
colourAttach.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
colourAttach.loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR;
colourAttach.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;
colourAttach.clearValue  = { .color = { 0.1f, 0.1f, 0.1f, 1.0f } }; // Dark grey

VkRenderingInfoKHR renderInfo{ VK_STRUCTURE_TYPE_RENDERING_INFO_KHR };
renderInfo.renderArea           = { {0, 0}, swapchain.extent() };
renderInfo.layerCount           = 1;
renderInfo.colorAttachmentCount = 1;
renderInfo.pColorAttachments    = &colourAttach;
// renderInfo.pDepthAttachment  = nullptr  (no depth for M1)

vkCmdBeginRendering(cmd, &renderInfo);
```

### 3g. destroy()

Reverse init order. Wait idle before destroying sync objects:

```cpp
vkDeviceWaitIdle(device);
for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    vkDestroySemaphore(device, m_imageAvailableSemaphores[i], nullptr);
    vkDestroySemaphore(device, m_renderFinishedSemaphores[i], nullptr);
    vkDestroyFence(device, m_inFlightFences[i], nullptr);
}
vkDestroyCommandPool(device, m_commandPool, nullptr);
```

---

## Step 4 — main.cpp

**File:** `src/main.cpp`

### 4a. GLFW init + window

```cpp
glfwInit();
glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // No OpenGL context
glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);   // Simplify M1 (add resize in M2)
GLFWwindow* window = glfwCreateWindow(1280, 720, "FYP Vulkan Renderer", nullptr, nullptr);
```

> `GLFW_NO_API` is mandatory — GLFW defaults to OpenGL, which conflicts with Vulkan.

### 4b. Init sequence

```cpp
VulkanContext ctx;
ctx.init(window);

SwapChain swap;
swap.init(ctx, 1280, 720);

Pipeline pipeline;
pipeline.init(ctx, "shaders/triangle.vert.spv", "shaders/triangle.frag.spv", swap.format());

Renderer renderer;
renderer.init(ctx, swap);
```

### 4c. Main loop

```cpp
while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
    renderer.drawFrame(ctx, swap, pipeline);
}
renderer.waitIdle(ctx);
```

### 4d. Teardown (reverse init order)

```cpp
renderer.destroy(ctx);
pipeline.destroy(ctx);
swap.destroy(ctx);
ctx.destroy();
glfwDestroyWindow(window);
glfwTerminate();
```

---

## Validation Layer Traps to Watch For

| Trap | Symptom | Fix |
|------|---------|-----|
| Wrong image layout in barrier | Validation error: layout mismatch | Check `oldLayout` matches current actual layout |
| Fence not started signalled | GPU hang on frame 0 | `VK_FENCE_CREATE_SIGNALED_BIT` in fenceInfo.flags |
| Shader module not destroyed | Memory leak warning | Destroy after `vkCreateGraphicsPipelines` |
| Swapchain format mismatch | Corrupt colours / validation error | Pass `swap.format()` to Pipeline::init |
| Missing `GLFW_NO_API` hint | GLFW/Vulkan surface error | Add before `glfwCreateWindow` |
| Dynamic state not set | Validation error: viewport/scissor not set | Call `vkCmdSetViewport`/`vkCmdSetScissor` every frame |
| `renderPass = VK_NULL_HANDLE` | Validation error about missing renderPass | Correct — Dynamic Rendering pipelines set renderPass = VK_NULL_HANDLE |

---

## Build & Run Checklist

```bash
# 1. Configure (first time only — downloads vcpkg deps)
cmake --preset linux-debug

# 2. Build (compiles shaders + C++ in one step)
cmake --build --preset linux-debug

# 3. Run WITH validation layers (mandatory during development)
VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation ./build/linux-debug/vulkan-renderer
```

**Success criteria for M1:**
- [ ] Window opens (1280 × 720)
- [ ] Dark grey background visible
- [ ] RGB-interpolated triangle rendered
- [ ] Zero validation layer errors in terminal output
- [ ] Clean exit on window close (no crashes, no leaks flagged by ASan)

---

## Wiring Diagram

```
main.cpp
  ├── glfwInit → window
  ├── VulkanContext::init(window)
  │     └── instance → surface → physicalDevice → device → queue
  ├── SwapChain::init(ctx, w, h)
  │     └── swapchain → images → imageViews
  ├── Pipeline::init(ctx, vert.spv, frag.spv, format)
  │     └── loadSpv → shaderModules → pipelineLayout → graphicsPipeline
  ├── Renderer::init(ctx, swap)
  │     └── commandPool → commandBuffers[2] → semaphores[2] → fences[2]
  │
  └── while(running):
        Renderer::drawFrame(ctx, swap, pipeline)
          wait fence[i]
          acquireNextImage → imageIndex
          record: barrier | beginRendering | draw | endRendering | barrier
          submit (semaphore sync)
          present
```

---

## After M1 Completes

Once the triangle renders cleanly:

1. Write dev log → `/docs/FYP-Vault/02-Dev-Log/2026-04-NN.md`
2. Move `Vulkan-M1-Deep-Dive.md` to `04-Archive/` (or tag as complete)
3. Begin M2 planning: rotating cube, UBO, depth buffer, `VkDescriptorSet`
4. Enable window resize handling (rebuild swapchain on `VK_ERROR_OUT_OF_DATE_KHR`)
