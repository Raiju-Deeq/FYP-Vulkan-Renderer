# FYP — Vulkan Renderer in C++20

**Mohamed Deeq Mohamed · P2884884 · De Montfort University · 2025–2026**

> A real-time Vulkan 1.3 renderer built from scratch in C++20, targeting Dynamic Rendering, explicit synchronisation, and physically based shading — with a stretch goal of 3D Gaussian Splatting (M6) to bridge into PhD research in neural rendering.

> Open this folder as your Obsidian vault: `File → Open vault → docs/FYP-Vault/`

---

## Quick Navigation

| Area | Link |
|------|------|
| Today's Dev Log | [[02-Dev-Log/2026-03-30]] |
| Current Plan | [[01-Plans/Week-1-Day-3-Plan]] |
| M1 Learnings | [[03-Learnings/Vulkan-Object-Chain]] |
| Gaussian Splatting | [[Research/Gaussian-Splatting]] |
| PBR Shading | [[Research/PBR-Shading]] |
| Inbox | [[00-Inbox/]] |
| Archive | [[04-Archive/]] |

---

## Project Overview

This project implements a Vulkan renderer that demonstrates mastery of modern GPU programming concepts at the lowest practical abstraction level. The renderer deliberately avoids legacy Vulkan patterns (`VkRenderPass`, `VkFramebuffer`) in favour of Vulkan 1.3 core features: **Dynamic Rendering**, **synchronization2**, and **VMA** for all GPU memory.

The project is scoped as a renderer, not an engine — no physics, ECS, audio, or scripting. The architectural focus is correctness, explicitness, and reproducibility.

**Repository:** [FYP-Vulkan-Renderer on GitHub](https://github.com/Raiju-Deeq/FYP-Vulkan-Renderer)

---

## MoSCoW Milestones

| Milestone | Description | Priority | Target Week | Status |
|-----------|-------------|----------|-------------|--------|
| M1 | Baseline pipeline + triangle (Dynamic Rendering) | Must | Wk 2 — 13 Apr | In Progress |
| M2 | Rotating 3D cube (UBO + depth buffer) | Must | Wk 3 — 20 Apr | Pending |
| M3 | OBJ loading + textures + Dear ImGui | Should | Wk 5 — 4 May | Pending |
| M4 | PBR shading (Cook–Torrance BRDF) | Could | Wk 7 — 18 May | Pending |
| M5 | Polish + full 8-section report | Must | Wk 9 — 1 Jun | Pending |
| M6 | 3D Gaussian Splatting (.ply render) | Could | Post-M3 | Stretch |

> M6 is hard-gated behind M3 sign-off and cannot begin until then.

---

## Architecture

| Module | Role |
|--------|------|
| `VulkanContext` | Instance, physical/logical device, graphics queue (via vk-bootstrap) |
| `SwapChain` | Swapchain, `VkImage[]`, `VkImageView[]` (via vk-bootstrap) |
| `Pipeline` | SPIR-V loading, `VkPipeline`, `VkPipelineLayout`, `VkDescriptorSetLayout` |
| `Renderer` | Per-frame loop: command pool/buffers, semaphores, fences, `drawFrame()` |
| `Mesh` | Vertex/index buffers (device-local, allocated via VMA) |
| `Material` | PBR uniform buffer, descriptor set, textures (`VkImage`/`VkSampler`) |
| `GaussianSplat` | GPU storage buffer for 3D Gaussian splatting (M6 stretch goal) |

### Key Design Decisions

- **Dynamic Rendering only** — no `VkRenderPass`, no `VkFramebuffer`. Uses `vkCmdBeginRendering`/`vkCmdEndRendering` (Vulkan 1.3 core).
- **Explicit synchronisation** — all image layout transitions use `VkImageMemoryBarrier2` + `synchronization2`. No hidden state.
- **Double-buffered** — `MAX_FRAMES_IN_FLIGHT = 2`; per-frame semaphores + fences throttle CPU ahead of GPU.
- **VMA for all GPU memory** — no direct `vkAllocateMemory` calls anywhere.
- **vk-bootstrap** — handles instance/device/swapchain boilerplate.

---

## Tech Stack

| Tool | Purpose |
|------|---------|
| Vulkan 1.3 (LunarG SDK) | Graphics API |
| C++20 | Language standard (RAII throughout) |
| vk-bootstrap | Instance / device / swapchain setup |
| GLFW3 | Window + input |
| GLM | Maths (depth `[0,1]`, radians enforced) |
| VulkanMemoryAllocator | All GPU memory allocation |
| Dear ImGui | Debug overlay (GLFW + Vulkan backends) |
| tinyobjloader | OBJ mesh loading (M3) |
| stb_image | Texture loading (M3) |
| spdlog | Logging |
| CMake + vcpkg | Build system + dependency management |
| Doxygen | API documentation (auto-generated) |

---

## Risk Register

| # | Risk | Likelihood | Impact | Mitigation |
|---|------|------------|--------|------------|
| 1 | Report deprioritised in favour of artefact | High | High | Start report in Week 4 |
| 2 | Vulkan sync errors eating development time | Medium | Medium | Validation layers always on |
| 3 | Image layout transition errors (Dynamic Rendering) | Medium | Medium | Reusable `transitionImageLayout` helper from M1 |
| 4 | M6 causing scope creep | Medium | Medium | Hard gate behind M3; dropped immediately if M3 slips |
| 5 | vk-bootstrap swapchain incompatibility on DMU lab drivers | Low | Medium | Fallback control exposed, verified before M1 sign-off |
| 6 | CMake/vcpkg build failures across platforms | Low | Low | Both presets verified on clean environments |
| 7 | Issues building for multiple platforms (Arch at home, Windows at Uni) | Medium-High | High | Prioritize working at Uni Campus if this issue becomes big |
| 8 | `.ply` parser perf for large splat files | Low | Low | Cap at 1M splats for M6 scope |

---

## Won't Have (Intentional Scope Decisions)

- Not a game engine — no physics, ECS, audio, scripting, or editor tooling
- No shadow mapping, deferred shading, skeletal animation, or mobile/console support
- No NeRF training or ML inference — M6 renders pre-trained `.ply` files only
- No `VK_KHR_ray_tracing_pipeline` in this submission (future work / PhD research)
- No multi-GPU, no Vulkan portability layer, no Android/iOS support

---

## Vault Structure

| Folder | Purpose |
|--------|---------|
| `00-Inbox/` | Unprocessed notes — sort weekly |
| `01-Plans/` | Milestone roadmaps and daily session plans |
| `02-Dev-Log/` | Daily development logs (generated via `/devlog`) |
| `03-Learnings/` | Concept notes and Vulkan deep-dives |
| `04-Archive/` | Completed milestones and reference material |
| `Research/` | In-depth research: Gaussian splatting, PBR, neural rendering |
| `Canvas/` | Visual planning boards |

---

## Claude Code Slash Commands

```
/devlog           → generates today's log → 02-Dev-Log/YYYY-MM-DD.md
/note [topic]     → creates research note → 03-Learnings/[topic].md
/plan [task]      → creates plan doc      → 01-Plans/[task].md
/report [section] → drafts report section from dev logs
/docme [file]     → adds/fixes Doxygen comments on source file
/validate         → runs cmake build + validation layers
```

---

## Weekly Timeline

| Week | Dates | Focus |
|------|-------|-------|
| 1 | 28 Mar – 3 Apr | Setup, documentation, architecture, concept learning |
| 2 | 6 Apr – 13 Apr | **M1** — Triangle via Dynamic Rendering |
| 3 | 14 Apr – 20 Apr | **M2** — Rotating cube, UBO, depth buffer |
| 4–5 | 21 Apr – 4 May | **M3** — OBJ loading, textures, Dear ImGui; begin report |
| 6–7 | 5 May – 18 May | **M4** — PBR shading (if M3 complete) |
| 8 | 19 May – 25 May | **M6** — Gaussian splatting (if M3 complete); report writing |
| 9 | 26 May – 1 Jun | **M5** — Polish, RenderDoc profiling, final report |

---

*FYP — Vulkan Renderer in C++20 · Mohamed Deeq Mohamed · P2884884 · De Montfort University*
