# CLAUDE.md

**Tradeoff:** These guidelines bias toward caution over speed. For trivial tasks, use judgment.

---

## Build Commands

**Linux (Arch — home machine):**
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

:: Release build
cmake --preset uni-release
cmake --build --preset uni-release
```

Debug presets (`linux-debug`, `uni-debug`) enable AddressSanitizer + UBSan automatically.

---

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
| `Material` | Texture, basic lighting UBO, and descriptor set (`VkImage`/`VkSampler`); PBR params available for C2 |
| `GaussianSplat` | GPU storage buffer for 3D Gaussian splatting (C3 stretch goal) |

### Key Design Decisions

- **Dynamic Rendering only** — no `VkRenderPass`, no `VkFramebuffer`. Uses `vkCmdBeginRendering`/`vkCmdEndRendering` (Vulkan 1.3 core). Attachments described inline at command recording time.
- **Explicit synchronisation** — all image layout transitions use `VkImageMemoryBarrier2` + `synchronization2`. No hidden state.
- **Double-buffered** — `MAX_FRAMES_IN_FLIGHT = 2`; per-frame semaphores + fences throttle CPU ahead of GPU.
- **VMA for all GPU memory** — no direct `vkAllocateMemory` calls anywhere.
- **vk-bootstrap** — handles instance/device/swapchain boilerplate so implementation focus stays on rendering logic.

### GLM Configuration (defined globally in CMake)

```cpp
GLM_FORCE_DEPTH_ZERO_TO_ONE   // Vulkan depth range , not OpenGL [-1,1]
GLM_FORCE_RADIANS
GLM_ENABLE_EXPERIMENTAL
```

### Shaders

GLSL source files live in `shaders/`. CMake compiles them to `.spv` via `glslc` automatically on build. Compiled `.spv` files are gitignored.

| Shader pair | Milestone |
|-------------|-----------|
| `triangle.vert/frag` | M1 — baseline triangle |
| `mesh.vert` + `mesh.frag` | M2–M3 — geometry + basic lighting |
| `mesh.vert` + `pbr.frag` | C2 — PBR upgrade (Could Have) |
| `splat.vert/frag` | C3 — Gaussian splatting (Could Have) |

---

## Dependencies

Managed via **vcpkg manifest mode** (`vcpkg.json`). All dependencies download automatically on first `cmake --preset`. Do not manually install them.

Core: Vulkan 1.3 (LunarG SDK), vk-bootstrap, GLFW3, GLM, VulkanMemoryAllocator, stb_image, tinyobjloader, Dear ImGui (GLFW + Vulkan backends), spdlog.

---

## Validation Layers

Must remain **enabled throughout development** (not just for debugging). Always run with `VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation`. This is an explicit project requirement.

---

## Documentation

All public APIs carry Doxygen Javadoc-style headers. When adding new functions or classes, maintain this standard. Docs are generated via the `docs` CMake target.

---

## Project Status

Source files are actively implemented across all modules (`VulkanContext`, `SwapChain`, `Pipeline`, `Renderer`, `Mesh`, `Material`, `GaussianSplat`). See `README.md` for the full MoSCoW milestone breakdown (M1–M4, S1–S2, C1–C3) and weekly roadmap.

---

## Behavioural Rules

When asked to implement something:
1. Always write Doxygen `///` comments first — `@file`, `@brief`, `@param`, `@return` on every public method.
2. Use RAII C++20 — wrap Vulkan handles in classes with destructors.
3. Never suggest `VkRenderPass` or `VkFramebuffer` — Dynamic Rendering only.
4. Add validation layer checks — zero errors is mandatory.
5. Reference the correct CMake presets (`linux-debug`, `uni-debug`) in all build instructions.

---

## Coding Guidelines

### 1. Think Before Coding

**Don't assume. Don't hide confusion. Surface tradeoffs.**

Before implementing:
- State your assumptions explicitly. If uncertain, ask.
- If multiple interpretations exist, present them — don't pick silently.
- If a simpler approach exists, say so. Push back when warranted.
- If something is unclear, stop. Name what's confusing. Ask.

### 2. Simplicity First

**Minimum code that solves the problem. Nothing speculative.**

- No features beyond what was asked.
- No abstractions for single-use code.
- No "flexibility" or "configurability" that wasn't requested.
- No error handling for impossible scenarios.
- If you write 200 lines and it could be 50, rewrite it.

Ask yourself: "Would a senior engineer say this is overcomplicated?" If yes, simplify.

### 3. Surgical Changes

**Touch only what you must. Clean up only your own mess.**

When editing existing code:
- Don't "improve" adjacent code, comments, or formatting.
- Don't refactor things that aren't broken.
- Match existing style, even if you'd do it differently.
- If you notice unrelated dead code, mention it — don't delete it.

When your changes create orphans:
- Remove imports/variables/functions that YOUR changes made unused.
- Don't remove pre-existing dead code unless asked.

The test: every changed line should trace directly to the user's request.

### 4. Goal-Driven Execution

**Define success criteria. Loop until verified.**

Transform tasks into verifiable goals:
- "Add validation" → "Write tests for invalid inputs, then make them pass"
- "Fix the bug" → "Write a test that reproduces it, then make it pass"
- "Refactor X" → "Ensure tests pass before and after"

For multi-step tasks, state a brief plan before starting:

    [Step] → verify: [check]

    [Step] → verify: [check]

    [Step] → verify: [check]

text

Strong success criteria let you loop independently. Weak criteria ("make it work") require constant clarification.

---

## Learning Workflow

When asked to explain or teach something new:
1. Explain the concept and "why" first.
2. Provide 2–3 curated YouTube videos + vkguide.dev or Vulkan Tutorial links.
3. Guide through a code implementation step-by-step.
4. Remind to write a dev log entry after.

---

## Dev Log Workflow

After every coding session, ask:
> "Ready to write your dev log entry? I can generate a template in `docs/FYP-Vault/02-Dev-Log/YYYY-MM-DD.md`"

---

## Vault Awareness

All project notes live in `docs/FYP-Vault/`. When creating or editing notes:
1. Use Obsidian Flavored Markdown (wikilinks, callouts, front-matter properties).
2. Dev logs go in `docs/FYP-Vault/02-Dev-Log/YYYY-MM-DD.md`.
3. Research notes go in `docs/FYP-Vault/Research/`.
4. Planning docs go in `docs/FYP-Vault/01-Plans/`.
5. Always add YAML front-matter with `date`, `tags`, and `status` fields.

### Vault Note Template

When generating any vault note, use this front-matter block:

```yaml
---
date: YYYY-MM-DD
tags: [vulkan, fyp, <topic>]
status: draft | in-progress | complete
---
```

---

## Slash Commands

| Command | Action |
|---------|--------|
| `/devlog` | Generate today's dev log from git diff + session context → `docs/FYP-Vault/02-Dev-Log/YYYY-MM-DD.md` |
| `/note [topic]` | Create a research note in `docs/FYP-Vault/Research/<topic>.md` with proper front-matter |
| `/plan [task]` | Create a planning doc in `docs/FYP-Vault/01-Plans/<task>.md` with milestones and steps |
| `/docme [file]` | Add/fix Doxygen comments on the specified file |
| `/validate` | Run cmake build + validation layers and report errors |
| `/report [section]` | Draft a report section paragraph from dev logs in `docs/FYP-Vault/02-Dev-Log/` |
