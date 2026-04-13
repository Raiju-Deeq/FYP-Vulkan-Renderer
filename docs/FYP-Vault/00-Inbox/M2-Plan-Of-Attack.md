---
date: 2026-04-13
tags: [vulkan, fyp, milestone-2, plan, vma, descriptors, depth-buffer]
status: in-progress
---

# M2 Plan of Attack — Rotating 3D Cube

> [!abstract] Goal
> By the end of this session: a 3D cube rotating on screen, with correct depth occlusion, driven by a UBO MVP matrix. Zero validation errors.

**Full reference doc:** [[M2-Rotating-Cube]] (in Plans/) — detailed code snippets for every step.
**Blocked on:** Nothing. M1 is complete and clean.

---

## Before You Touch Code

Run the existing build to confirm M1 still works:
```bash
cmake --build --preset linux-debug
VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation ./build/linux-debug/vulkan-renderer
```
You should see: triangle renders, zero validation errors, clean shutdown.

---

## Implementation Order

Stick to this order — each step unblocks the next.

```
Step 1: VulkanContext — add VMA allocator + transfer command pool
Step 2: SwapChain     — add depth image + depth image view
Step 3: Mesh          — implement upload(), bind(), draw(), destroy()
Step 4: Pipeline      — vertex input bindings, descriptor set layout, depth state
Step 5: Renderer      — UBO buffers, descriptor sets, updated drawFrame()
Step 6: main.cpp      — cube geometry, angle tracking, wire it all together
```

---

## Step 1 — VulkanContext: Add VMA

**Files:** `src/VulkanContext.h`, `src/VulkanContext.cpp`

Add to `VulkanContext.h`:
```cpp
#include <vk_mem_alloc.h>

// private:
VmaAllocator m_allocator    = VK_NULL_HANDLE;
VkCommandPool m_transferPool = VK_NULL_HANDLE;

// public accessors:
VmaAllocator  allocator()     const { return m_allocator; }
VkCommandPool transferPool()  const { return m_transferPool; }
```

Add to `VulkanContext.cpp` `init()` — after Stage 4 (device creation):
```cpp
// VMA
VmaAllocatorCreateInfo allocatorInfo{};
allocatorInfo.instance         = m_instance;
allocatorInfo.physicalDevice   = m_physicalDevice;
allocatorInfo.device           = m_device;
allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
allocatorInfo.flags            = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
if (vmaCreateAllocator(&allocatorInfo, &m_allocator) != VK_SUCCESS) {
    spdlog::error("VulkanContext: VMA init failed");
    return false;
}

// Transfer command pool (for one-time upload commands in Mesh::upload)
VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
poolInfo.queueFamilyIndex = m_graphicsQueueFamily;
poolInfo.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_transferPool);
```

In `destroy()` — before device destruction:
```cpp
if (m_transferPool != VK_NULL_HANDLE) {
    vkDestroyCommandPool(m_device, m_transferPool, nullptr);
}
if (m_allocator != VK_NULL_HANDLE) {
    vmaDestroyAllocator(m_allocator);
}
```

> [!warning] VMA must be destroyed BEFORE the device. Order matters.

**Verify:** Build compiles, triangle still renders, no new validation errors.

---

## Step 2 — SwapChain: Depth Image

**Files:** `src/SwapChain.h`, `src/SwapChain.cpp`

Add to `SwapChain.h`:
```cpp
#include <vk_mem_alloc.h>

static constexpr VkFormat DEPTH_FORMAT = VK_FORMAT_D32_SFLOAT;

VkImageView depthView() const { return m_depthView; }
// also add: VkImage depthImage() const { return m_depthImage; }

// private:
VkImage       m_depthImage      = VK_NULL_HANDLE;
VkImageView   m_depthView       = VK_NULL_HANDLE;
VmaAllocation m_depthAllocation = nullptr;
```

`init()` needs `const VulkanContext& ctx` passed in (check if it already does — update call sites in main.cpp if needed).

At the end of `SwapChain::init()`, after creating colour image views:
```cpp
VkImageCreateInfo depthInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
depthInfo.imageType   = VK_IMAGE_TYPE_2D;
depthInfo.format      = DEPTH_FORMAT;
depthInfo.extent      = {m_extent.width, m_extent.height, 1};
depthInfo.mipLevels = depthInfo.arrayLayers = 1;
depthInfo.samples   = VK_SAMPLE_COUNT_1_BIT;
depthInfo.tiling    = VK_IMAGE_TILING_OPTIMAL;
depthInfo.usage     = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

VmaAllocationCreateInfo depthAllocInfo{};
depthAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
vmaCreateImage(ctx.allocator(), &depthInfo, &depthAllocInfo,
               &m_depthImage, &m_depthAllocation, nullptr);

VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
viewInfo.image    = m_depthImage;
viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
viewInfo.format   = DEPTH_FORMAT;
viewInfo.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
vkCreateImageView(ctx.device(), &viewInfo, nullptr, &m_depthView);
```

In `SwapChain::destroy()`:
```cpp
vkDestroyImageView(ctx.device(), m_depthView, nullptr);
vmaDestroyImage(ctx.allocator(), m_depthImage, m_depthAllocation);
```

**Verify:** Build clean. No new validation errors. (Depth image won't be used yet until Step 5.)

---

## Step 3 — Mesh: Staging Upload

**Files:** `src/Mesh.h`, `src/Mesh.cpp`

Update `Mesh.h` — add VMA allocation members and update signatures:
```cpp
#include <vk_mem_alloc.h>
class VulkanContext; // forward declare at top

// Change upload/destroy signatures:
bool upload(const VulkanContext&        ctx,
            const std::vector<Vertex>&   vertices,
            const std::vector<uint32_t>& indices);
void destroy(const VulkanContext& ctx);

// private: add alongside existing VkBuffer members
VmaAllocation m_vertexAllocation = nullptr;
VmaAllocation m_indexAllocation  = nullptr;
```

Implement in `Mesh.cpp` — see full pseudocode already in the file header.

**Key pattern for each buffer (vertex and index):**
```
1. make staging buffer  (VMA_MEMORY_USAGE_AUTO + HOST_ACCESS_SEQUENTIAL_WRITE + MAPPED)
2. memcpy from CPU vector via pMappedData (no map/unmap — it's persistently mapped)
3. make device-local buffer (VMA_MEMORY_USAGE_AUTO + TRANSFER_DST + VERTEX/INDEX usage)
4. one-time command buffer: vkCmdCopyBuffer(staging → device-local)
5. vkQueueSubmit + vkQueueWaitIdle
6. vmaDestroyBuffer(staging)
```

Helper for the one-time submit (put it at the top of Mesh.cpp as a static function):
```cpp
static void oneTimeSubmit(const VulkanContext& ctx,
                          std::function<void(VkCommandBuffer)> fn)
{
    VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ai.commandPool        = ctx.transferPool();
    ai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;
    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(ctx.device(), &ai, &cmd);

    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin);
    fn(cmd);
    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.commandBufferCount = 1;
    submit.pCommandBuffers    = &cmd;
    vkQueueSubmit(ctx.graphicsQueue(), 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(ctx.graphicsQueue());
    vkFreeCommandBuffers(ctx.device(), ctx.transferPool(), 1, &cmd);
}
```

bind() and draw() — simple:
```cpp
void Mesh::bind(VkCommandBuffer cmd) const {
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &m_vertexBuffer, &offset);
    vkCmdBindIndexBuffer(cmd, m_indexBuffer, 0, VK_INDEX_TYPE_UINT32);
}
void Mesh::draw(VkCommandBuffer cmd) const {
    vkCmdDrawIndexed(cmd, m_indexCount, 1, 0, 0, 0);
}
```

**Verify:** Unit test by uploading the cube from main.cpp (Step 6) before worrying about rendering.

---

## Step 4 — Pipeline: Vertex Input + Descriptor Layout + Depth

**File:** `src/Pipeline.cpp`  (and `Pipeline.h` for descriptor layout accessor)

Three changes to the existing `init()`:

**4a — Replace the empty vertex input struct:**
```cpp
VkVertexInputBindingDescription binding{};
binding.binding   = 0;
binding.stride    = sizeof(Vertex);
binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

VkVertexInputAttributeDescription attrs[3]{};
attrs[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position)};
attrs[1] = {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)};
attrs[2] = {2, 0, VK_FORMAT_R32G32_SFLOAT,    offsetof(Vertex, texCoord)};

VkPipelineVertexInputStateCreateInfo vertexInput{
    VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
vertexInput.vertexBindingDescriptionCount   = 1;
vertexInput.pVertexBindingDescriptions      = &binding;
vertexInput.vertexAttributeDescriptionCount = 3;
vertexInput.pVertexAttributeDescriptions    = attrs;
```

**4b — Create descriptor set layout (set 0 = MVP UBO):**
```cpp
VkDescriptorSetLayoutBinding uboBinding{};
uboBinding.binding        = 0;
uboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
uboBinding.descriptorCount = 1;
uboBinding.stageFlags     = VK_SHADER_STAGE_VERTEX_BIT;

VkDescriptorSetLayoutCreateInfo layoutInfo{
    VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
layoutInfo.bindingCount = 1;
layoutInfo.pBindings    = &uboBinding;
vkCreateDescriptorSetLayout(ctx.device(), &layoutInfo, nullptr, &m_descriptorSetLayout);
```
Update pipeline layout to reference it (`pSetLayouts = &m_descriptorSetLayout`).

**4c — Enable depth test:**
```cpp
VkPipelineDepthStencilStateCreateInfo depthStencil{
    VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
depthStencil.depthTestEnable  = VK_TRUE;
depthStencil.depthWriteEnable = VK_TRUE;
depthStencil.depthCompareOp   = VK_COMPARE_OP_LESS;
pipelineInfo.pDepthStencilState = &depthStencil;
```

**4d — Add depth format to the dynamic rendering chain:**
```cpp
renderingInfo.depthAttachmentFormat = SwapChain::DEPTH_FORMAT;
```

**4e — Switch to mesh.vert + pbr.frag** (replace "triangle" shader paths with "mesh" / "pbr").

**Verify:** Pipeline compiles. Shader modules load correctly.

---

## Step 5 — Renderer: UBOs + Descriptor Sets + Updated drawFrame

**Files:** `src/Renderer.h`, `src/Renderer.cpp`

Add to `Renderer.h`:
```cpp
#include <vk_mem_alloc.h>
#include <glm/glm.hpp>
class Mesh; // forward declare

// UBO struct (mirrors mesh.vert layout exactly):
struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

// private members:
VkDescriptorPool m_descriptorPool                         = VK_NULL_HANDLE;
VkDescriptorSet  m_descriptorSets[MAX_FRAMES_IN_FLIGHT]   = {};
VkBuffer         m_uboBuffers[MAX_FRAMES_IN_FLIGHT]       = {};
VmaAllocation    m_uboAllocations[MAX_FRAMES_IN_FLIGHT]   = {};
void*            m_uboMapped[MAX_FRAMES_IN_FLIGHT]        = {};
```

Update `init()` signature to take `const Pipeline& pipeline` (needed for the descriptor set layout).

In `init()` — add after command pool/buffers:
```cpp
// Descriptor pool
VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_FRAMES_IN_FLIGHT};
VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT;
poolInfo.poolSizeCount = 1;
poolInfo.pPoolSizes    = &poolSize;
vkCreateDescriptorPool(ctx.device(), &poolInfo, nullptr, &m_descriptorPool);

// Per-frame UBO buffers (persistently mapped — no map/unmap per frame)
for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    VkBufferCreateInfo bufInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufInfo.size  = sizeof(UniformBufferObject);
    bufInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    VmaAllocationCreateInfo ai{};
    ai.usage = VMA_MEMORY_USAGE_AUTO;
    ai.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
             | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    VmaAllocationInfo mapInfo;
    vmaCreateBuffer(ctx.allocator(), &bufInfo, &ai,
                    &m_uboBuffers[i], &m_uboAllocations[i], &mapInfo);
    m_uboMapped[i] = mapInfo.pMappedData;
}

// Allocate and write descriptor sets
VkDescriptorSetLayout layouts[MAX_FRAMES_IN_FLIGHT];
std::fill(std::begin(layouts), std::end(layouts), pipeline.descriptorSetLayout());
VkDescriptorSetAllocateInfo dsAlloc{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
dsAlloc.descriptorPool     = m_descriptorPool;
dsAlloc.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
dsAlloc.pSetLayouts        = layouts;
vkAllocateDescriptorSets(ctx.device(), &dsAlloc, m_descriptorSets);

for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    VkDescriptorBufferInfo bufInfo{m_uboBuffers[i], 0, sizeof(UniformBufferObject)};
    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet          = m_descriptorSets[i];
    write.dstBinding      = 0;
    write.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.descriptorCount = 1;
    write.pBufferInfo     = &bufInfo;
    vkUpdateDescriptorSets(ctx.device(), 1, &write, 0, nullptr);
}
```

Update `drawFrame()` to:
1. Accept `const Mesh& mesh` and `float angle` parameters
2. Write the UBO before recording (see Step 5 in M2-Rotating-Cube.md for the glm calls)
3. Add a depth barrier before `vkCmdBeginRendering`
4. Add depth attachment to `VkRenderingInfo`
5. Replace `vkCmdDraw(3)` with `vkCmdBindDescriptorSets` + `mesh.bind()` + `mesh.draw()`

**Y-flip reminder:** `ubo.proj[1][1] *= -1.0f` — GLM generates OpenGL clip space, Vulkan's Y axis is flipped.

**Depth barrier — transition depth image UNDEFINED → DEPTH_STENCIL_ATTACHMENT_OPTIMAL each frame** (discard old contents — we clear it anyway).

---

## Step 6 — main.cpp: Cube Geometry + Render Loop

Cube vertex data: 24 vertices (4 per face × 6 faces), 36 indices.
Full vertex/index arrays are in [[M2-Rotating-Cube]] → "Step 6 — Cube vertex data".

Add time-based rotation:
```cpp
#include <chrono>
auto startTime = std::chrono::high_resolution_clock::now();

// In the loop:
float t     = std::chrono::duration<float>(
    std::chrono::high_resolution_clock::now() - startTime).count();
float angle = t * glm::radians(90.0f); // 90 degrees per second
```

Upload mesh once before the loop:
```cpp
Mesh cube;
cube.upload(ctx, cubeVertices, cubeIndices);
// ... render loop ...
cube.destroy(ctx);
```

---

## Gotchas to Watch For

> [!bug] Chicken-and-egg: Pipeline before Renderer
> `Renderer::init()` needs `pipeline.descriptorSetLayout()` to allocate descriptor sets. Make sure `main.cpp` initialises in order: `VulkanContext → SwapChain → Pipeline → Renderer`. It already does — just add `Pipeline` as a parameter to `Renderer::init()`.

> [!bug] Y-flip or cube appears upside down
> `ubo.proj[1][1] *= -1.0f` — don't forget this. GLM was designed for OpenGL.

> [!bug] Depth barrier stage masks
> The depth barrier `dstStageMask` must be `EARLY_FRAGMENT_TESTS` not `LATE_FRAGMENT_TESTS`. Early is where depth reads happen; late is where writes are committed.

> [!bug] Resize — depth image must be rebuilt too
> When `drawFrame()` returns `VK_ERROR_OUT_OF_DATE_KHR`, call `swap.rebuild(ctx)`. The depth image is part of the swapchain now, so it rebuilds automatically with it.

---

## Verification Checklist

- [ ] Compiles clean — zero warnings/errors on `linux-debug`
- [ ] Cube renders — all 6 faces visible as rotation reveals them  
- [ ] Depth correct — back faces hidden (no Z-fighting or bleed-through)
- [ ] Rotation is smooth and time-based (not frame-rate-dependent)
- [ ] Window resize works — cube continues rotating after drag
- [ ] Zero validation errors — startup, runtime, shutdown
- [ ] Screenshot → `docs/screenshots/M2-cube.png`

---

## After M2 is Done

- Write dev log in `docs/FYP-Vault/02-Dev-Log/YYYY-MM-DD.md`
- Check this off as complete and archive it to `04-Archive/`
- Next up: M3 — OBJ loading + albedo texture via `Material` class

---

*FYP — Vulkan Renderer · Mohamed Deeq Mohamed · P2884884 · DMU*
