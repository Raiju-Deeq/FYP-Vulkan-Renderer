# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

**Linux (Arch):**
```bash
# Configure
cmake --preset linux-debug
cmake --preset linux-release

# Build
cmake --build --preset linux-debug
cmake --build --preset linux-release

# Run
./build/linux-debug/vulkan-renderer

# Run with Vulkan validation layers (always use during development)
VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation ./build/linux-debug/vulkan-renderer

# Build shaders only (GLSL → SPIR-V via glslc)
cmake --build --preset linux-debug --target shaders

# Generate Doxygen API docs
cmake --build --preset linux-debug --target docs
xdg-open docs/doxygen/html/index.html
```

**Windows (DMU lab):**
```bat
cmake --preset uni-debug
cmake --build --preset uni-debug
build\uni-debug\vulkan-renderer.exe
```

Debug presets (`linux-debug`, `uni-debug`) enable AddressSanitizer + UBSan automatically.

## Architecture

The renderer targets **Vulkan 1.3** with **C++20 RAII** throughout. All source lives in `src/`.

### Module Responsibilities

| Module | Role |
|--------|------|
| `VulkanContext` | Instance, physical/logical device, graphics queue (via vk-bootstrap) |
| `SwapChain` | Swapchain, `VkImage[]`, `VkImageView[]` (via vk-bootstrap) |
| `Pipeline` | SPIR-V loading, `VkPipeline`, `VkPipelineLayout`, `VkDescriptorSetLayout` |
| `Renderer` | Per-frame loop: command pool/buffers, semaphores, fences, `drawFrame()` |
| `Mesh` | Vertex/index buffers (device-local, allocated via VMA) |
| `Material` | PBR uniform buffer, descriptor set, textures (`VkImage`/`VkSampler`) |
| `GaussianSplat` | GPU storage buffer for 3D Gaussian splatting (Milestone 6 stretch goal) |

### Key Design Decisions

- **Dynamic Rendering only** — no `VkRenderPass`, no `VkFramebuffer`. Uses `vkCmdBeginRendering`/`vkCmdEndRendering` (Vulkan 1.3 core). Attachments described inline at command recording time.
- **Explicit synchronisation** — all image layout transitions use `VkImageMemoryBarrier2` + `synchronization2`. No hidden state.
- **Double-buffered** — `MAX_FRAMES_IN_FLIGHT = 2`; per-frame semaphores + fences throttle CPU ahead of GPU.
- **VMA for all GPU memory** — no direct `vkAllocateMemory` calls anywhere.
- **vk-bootstrap** — handles instance/device/swapchain boilerplate so implementation focus stays on rendering logic.

### GLM Configuration (defined globally in CMake)

```cpp
GLM_FORCE_DEPTH_ZERO_TO_ONE   // Vulkan depth range [0,1], not OpenGL [-1,1]
GLM_FORCE_RADIANS
GLM_ENABLE_EXPERIMENTAL
```

### Shaders

GLSL source files live in `shaders/`. CMake compiles them to `.spv` via `glslc` automatically on build. Compiled `.spv` files are gitignored.

| Shader pair | Milestone |
|-------------|-----------|
| `triangle.vert/frag` | M1 — baseline triangle |
| `mesh.vert` + `pbr.frag` | M2–M4 — geometry + PBR |
| `splat.vert/frag` | M6 — Gaussian splatting |

## Dependencies

Managed via **vcpkg manifest mode** (`vcpkg.json`). All dependencies are downloaded automatically on first `cmake --preset`. Do not manually install them.

Core: Vulkan 1.3 (LunarG SDK), vk-bootstrap, GLFW3, GLM, VulkanMemoryAllocator, stb_image, tinyobjloader, Dear ImGui (GLFW + Vulkan backends), spdlog.

## Validation Layers

Validation layers must remain **enabled throughout development** (not just for debugging). Always run with `VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation`. This is an explicit project requirement.

## Documentation

All public APIs carry Doxygen Javadoc-style headers. When adding new functions or classes, maintain this standard. Docs are generated via the `docs` CMake target.

## Project Status

All source files are currently **scaffolded stubs** (Week 1). Implementation begins Week 2 with Milestone 1 (triangle via Dynamic Rendering). See `README.md` for the full MoSCoW milestone breakdown (M1–M6) and weekly roadmap.

## BEHAVIOURAL RULES

When I ask you to implement something:
1. Always write Doxygen /// comments first — @file, @brief, @param, @return on every public method
2. Use RAII C++20 — wrap Vulkan handles in classes with destructors
3. Never suggest VkRenderPass or VkFramebuffer — Dynamic Rendering only
4. Add validation layer checks — zero errors is mandatory
5. Reference my CMake presets (linux-debug, uni-debug) in any build instructions

## LEARNING WORKFLOW

When I ask to learn something new:
1. Explain the concept and "why" first
2. Provide 2-3 curated YouTube videos + vkguide.dev or Vulkan Tutorial links
3. Then guide me through a code implementation step-by-step
4. Remind me to write a dev log entry after

## DEV LOG WORKFLOW

After every coding session, ask me:
> "Ready to write your dev log entry? I can generate a template in docs/Dev-Log/YYYY-MM-DD.md"

## SLASH COMMANDS

Define these shortcuts:
/devlog — Generate today's dev log entry from git diff + session context
/docme [file] — Add/fix Doxygen comments on the specified file
/validate — Run cmake build + validation layers and report errors
/report [section] — Draft a paragraph for the given report section from dev logs
/plan [task] — Outline steps to implement the task before doing it



## VAULT AWARENESS

All project notes live in `docs/vault/`. When creating or editing notes:
1. Use Obsidian Flavored Markdown (wikilinks, callouts, front-matter properties)
2. Dev logs go in `docs/vault/Dev-Log/YYYY-MM-DD.md`
3. Research notes go in `docs/vault/Research/`
4. Planning docs go in `docs/vault/Plans/`
5. Always add YAML front-matter with `date`, `tags`, and `status` fields

## VAULT NOTE TEMPLATE

When generating any vault note:
---
date: YYYY-MM-DD
tags: [vulkan, fyp, <topic>]
status: draft | in-progress | complete
---

## SLASH COMMANDS (extended)

/devlog       — Generate today's dev log from git diff + session context → docs/vault/Dev-Log/YYYY-MM-DD.md
/note [topic] — Create a research note in docs/vault/Research/<topic>.md with proper front-matter
/plan [task]  — Create a planning doc in docs/vault/Plans/<task>.md with milestones and steps
/docme [file] — Add/fix Doxygen comments on the specified file
/validate     — Run cmake build + validation layers and report errors
/report [sec] — Draft a report section paragraph from dev logs in docs/vault/Dev-Log/
