<div align="center">

<br />

# Vulkan Renderer — C++20

<br />

*BSc (Hons) Games Production · Final Year Project*  
*De Montfort University, Leicester · Academic Year 2025–2026*

<br />

**Mohamed Deeq Mohamed** · P2884884  
Supervisors: Salim Hashu · Dr Conor Fahy

<br />

![C++](https://img.shields.io/badge/C%2B%2B-20-00599C?style=flat-square&logo=cplusplus&logoColor=white)
![Vulkan](https://img.shields.io/badge/Vulkan-1.3-AC162C?style=flat-square&logo=vulkan&logoColor=white)
![CMake](https://img.shields.io/badge/CMake-3.25%2B-064F8C?style=flat-square&logo=cmake&logoColor=white)
![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20Windows-lightgrey?style=flat-square&logo=linux&logoColor=white)
![Doxygen](https://img.shields.io/badge/Docs-Doxygen-2C4AA8?style=flat-square)
![Obsidian](https://img.shields.io/badge/Notes-Obsidian-7C3AED?style=flat-square&logo=obsidian&logoColor=white)
![Status](https://img.shields.io/badge/Status-Week%201%20of%209-orange?style=flat-square)

<br />

> *A real-time Vulkan 1.3 renderer built from scratch in C++20, targeting Dynamic Rendering,*  
> *explicit GPU synchronisation, and physically based shading —*  
> *with a stretch goal of real-time 3D Gaussian Splatting as a pilot for PhD research in neural rendering.*

<br />

</div>

---

## Table of Contents

- [Project Overview](#project-overview)
- [Knowledge System](#knowledge-system)
- [Architecture](#architecture)
- [MoSCoW Prioritisation](#moscow-prioritisation)
- [Milestone Roadmap](#milestone-roadmap)
- [Tech Stack](#tech-stack)
- [Getting Started](#getting-started)
- [CMake Presets](#cmake-presets)
- [API Documentation (Doxygen)](#api-documentation-doxygen)
- [Project Layout](#project-layout)
- [Technical Report](#technical-report)
- [Development Log](#development-log)
- [Academic Context](#academic-context)

---

## Project Overview

This project is a real-time 3D renderer implemented from first principles using the **Vulkan 1.3 API** and **C++20**, submitted as the Final Year Project for BSc (Hons) Games Production at De Montfort University. It is intentionally scoped as a **renderer, not an engine** — no physics, ECS, audio, or scene graph. Those are deliberate omissions, not gaps.

The renderer is built entirely around **Vulkan 1.3 Dynamic Rendering** — no `VkRenderPass`, no `VkFramebuffer`, ever. Colour and depth attachments are described inline at command recording time via `vkCmdBeginRendering`, and all image layout transitions are managed explicitly through `VkImageMemoryBarrier2` (synchronization2). Vulkan initialisation (instance, device, swapchain) is handled by **vk-bootstrap**, focusing development effort on the rendering systems that matter.

Beyond the degree, this project serves as a **pilot study for planned PhD research in neural rendering at De Montfort University**, and is deliberately architected to accommodate future work with `VK_KHR_ray_tracing_pipeline` and GPU-accelerated synthetic data pipelines.

```
Baseline Pipeline → 3D Geometry → GPU Memory → Texture Mapping
     → (Could Have) PBR Shading → (Stretch) 3D Gaussian Splatting
```

---

## Knowledge System

This project uses an integrated knowledge management system to maintain rigorous documentation throughout the development cycle. The system connects source code, API documentation, and project notes into a single coherent record.

```
┌─────────────────────────────────────────────────────────────┐
│                    Knowledge Pipeline                       │
│                                                             │
│  Source Code  ──→  Doxygen  ──→  API Reference (HTML)      │
│      │                                                      │
│      │          Claude Code  ──→  Session Assistance        │
│      │               │                                      │
│      └──────────────→│──→  Obsidian Vault                   │
│                       │         ├── 02-Dev-Log/             │
│                       │         ├── 01-Plans/               │
│                       │         ├── 03-Learnings/           │
│                       │         └── Research/               │
│                       │                                      │
│                       └──→  Technical Report                │
└─────────────────────────────────────────────────────────────┘
```

### Obsidian Vault

All project notes, dev logs, plans, and research are maintained in an [Obsidian](https://obsidian.md/) vault at [`docs/FYP-Vault/`](docs/FYP-Vault/). The vault is version-controlled alongside the source code, providing a persistent, linked knowledge base that feeds directly into the technical report.

| Section | Location | Purpose |
|---------|----------|---------|
| **Home** | [`docs/FYP-Vault/Home.md`](docs/FYP-Vault/Home.md) | Vault index — overview, MoSCoW, architecture, risk register |
| **Dev Logs** | [`docs/FYP-Vault/02-Dev-Log/`](docs/FYP-Vault/02-Dev-Log/) | Daily session records — feeds Report §5 & §6 |
| **Plans** | [`docs/FYP-Vault/01-Plans/`](docs/FYP-Vault/01-Plans/) | Milestone implementation plans |
| **Learnings** | [`docs/FYP-Vault/03-Learnings/`](docs/FYP-Vault/03-Learnings/) | Concept notes — Vulkan deep-dives |
| **Research** | [`docs/FYP-Vault/Research/`](docs/FYP-Vault/Research/) | Gaussian splatting, PBR, neural rendering |

Open the vault in Obsidian: **File → Open vault → `docs/FYP-Vault/`**

### Doxygen API Reference

All public APIs are documented with Doxygen Javadoc-style headers. The generated HTML reference is built on demand (not committed — see [API Documentation](#api-documentation-doxygen)).

### Claude Code Integration

The project ships with a `.claude/` skills directory enabling [Claude Code](https://claude.ai/code) to work with Obsidian Flavored Markdown, generate dev logs, and assist with implementation — all from the repo root. See [`CLAUDE.md`](CLAUDE.md) for the full specification.

---

## Architecture

### Module Responsibilities

| Module | Role |
|--------|------|
| `VulkanContext` | Instance, physical/logical device, graphics queue (via vk-bootstrap) |
| `SwapChain` | Swapchain, `VkImage[]`, `VkImageView[]`, resize handling |
| `Pipeline` | SPIR-V loading, `VkPipeline`, `VkPipelineLayout`, `VkDescriptorSetLayout` |
| `Renderer` | Per-frame loop: command pool/buffers, semaphores, fences, `drawFrame()` |
| `Mesh` | Vertex/index buffers (device-local, allocated via VMA) |
| `Material` | PBR uniform buffer, descriptor set, textures (`VkImage` / `VkSampler`) |
| `GaussianSplat` | GPU storage buffer for 3D Gaussian splatting — M6 stretch goal |

### Key Design Decisions

**Dynamic Rendering only** — `vkCmdBeginRendering` / `vkCmdEndRendering` throughout. Zero `VkRenderPass` or `VkFramebuffer` objects. This reflects current industry direction and simplifies the path toward M6 and future ray tracing work.

**Explicit synchronisation** — all image layout transitions via `VkImageMemoryBarrier2` (synchronization2). No hidden state, no driver assumptions. Every transition is documented with its source and destination stage/access masks.

**VMA for all GPU memory** — no direct `vkAllocateMemory` calls. Vertex, index, uniform, texture, depth, and splat buffers all go through Vulkan Memory Allocator, ensuring correct alignment and suballocation.

**RAII C++20** — Vulkan handles are wrapped in classes with destructors. Object lifetime and ownership are always explicit.

**Double-buffered** — `MAX_FRAMES_IN_FLIGHT = 2`. Per-frame semaphores and fences throttle the CPU so it cannot race more than one frame ahead of the GPU.

### Shader Pipeline

| Shader Pair | Milestone | Purpose |
|-------------|-----------|----------|
| `triangle.vert/frag` | M1 | Baseline Dynamic Rendering validation |
| `mesh.vert` + `pbr.frag` | M2–M4 | Geometry, UVs, Cook–Torrance BRDF |
| `splat.vert/frag` | M6 | Gaussian ellipse projection + alpha compositing |

All GLSL sources live in `shaders/`. CMake compiles them to `.spv` via `glslc` at build time. Compiled `.spv` files are gitignored.

---

## MoSCoW Prioritisation

### 🔴 Must Have
- **M1** — Vulkan 1.3 environment, triangle via Dynamic Rendering, zero validation errors
- **M2** — Rotating 3D cube, VMA vertex/index buffers, depth image, MVP UBO
- RAII C++20 codebase with Doxygen-documented public API throughout
- Reproducible CMake + vcpkg build on Linux (Arch) and Windows (DMU lab)
- Public GitHub repository with a sustained, meaningful commit history
- Technical report: all 8 sections complete

### 🟡 Should Have
- **M3** — OBJ mesh loading, texture mapping with mipmaps, Dear ImGui runtime panel
- Interactive orbit / FPS camera
- Swapchain resize handled cleanly via vk-bootstrap

### 🟢 Could Have
- **M4** — PBR shading: Cook–Torrance BRDF (GGX NDF, Smith G, Schlick Fresnel), Blender Cycles visual reference
- **M6** — 3D Gaussian Splatting: `.ply` loader, GPU splat buffers, depth sort, screen-space projection, alpha compositing
- RenderDoc performance captures at each milestone

### ⬜ Won't Have *(intentional scope decisions)*
Physics, ECS, audio, shadow mapping, deferred shading, skeletal animation, NeRF training, `VK_KHR_ray_tracing_pipeline`, multi-GPU, mobile/console support.

---

## Milestone Roadmap

| Milestone | Description | Priority | Target | Status |
|-----------|-------------|----------|--------|--------|
| **M1** | Baseline Vulkan 1.3 pipeline + triangle | 🔴 Must | Wk 2 — 13 Apr | ⏳ In Progress |
| **M2** | Rotating 3D cube + VMA buffers | 🔴 Must | Wk 3 — 20 Apr | ⏳ Pending |
| **M3** | OBJ loading + texture mapping + ImGui | 🟡 Should | Wk 5 — 4 May | ⏳ Pending |
| **M4** | PBR shading (Cook–Torrance BRDF) | 🟢 Could | Wk 7 — 18 May | ⏳ Pending |
| **M5** | Polish + technical report | 🔴 Must | Wk 9 — 1 Jun | ⏳ Pending |
| **M6** | 3D Gaussian Splatting (`.ply` → GPU) | 🟢 Could | Post M3 | ⏳ Stretch |

> M6 is hard-gated behind M3 sign-off. M4 and M6 are independent — either can be pursued first depending on progress after M3.

---

## Tech Stack

| Tool / Library | Purpose |
|----------------|---------|
| **Vulkan 1.3** (LunarG SDK) | Core graphics API |
| **vk-bootstrap** | Instance, device, swapchain initialisation |
| **C++20** | Implementation language (RAII, concepts, ranges) |
| **GLFW 3** | Window creation and input handling |
| **GLM** | Mathematics — depth `[0,1]`, radians enforced via CMake defines |
| **Vulkan Memory Allocator** | All GPU memory allocation and suballocation |
| **stb_image** | Texture and image loading |
| **tinyobjloader** | OBJ mesh loading (M3) |
| **Dear ImGui** | Runtime debug overlay (GLFW + Vulkan backends) |
| **spdlog** | Structured, levelled logging |
| **glslc** | GLSL → SPIR-V compilation (integrated via CMake) |
| **Doxygen** | API reference generation from source |
| **Obsidian** | Project knowledge base and dev log vault |
| **Claude Code** | AI-assisted development and note generation |
| **CMake 3.25+** | Build system |
| **vcpkg** | Dependency management (manifest mode) |

---

## Getting Started

Requires a **Vulkan 1.3-capable GPU**, a C++20 toolchain, and CMake 3.25+. Tested on Arch Linux and Windows (DMU lab machines).

### Clone

```bash
git clone https://github.com/Raiju-Deeq/FYP-Vulkan-Renderer.git
cd FYP-Vulkan-Renderer
```

### Linux (Arch — home machine)

```bash
# Install system dependencies
sudo pacman -S vulkan-radeon vulkan-validation-layers cmake ninja git \
               base-devel autoconf autoconf-archive automake libtool

# Bootstrap vcpkg
git clone https://github.com/microsoft/vcpkg "$HOME/vcpkg"
"$HOME/vcpkg/bootstrap-vcpkg.sh"
export VCPKG_ROOT="$HOME/vcpkg"

# Configure, build, run
cmake --preset linux-debug
cmake --build --preset linux-debug
VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation ./build/linux-debug/vulkan-renderer
```

### Windows (DMU lab — no admin required)

```bat
rem Bootstrap vcpkg (user-local, no admin needed)
git clone https://github.com/microsoft/vcpkg %USERPROFILE%\vcpkg
%USERPROFILE%\vcpkg\bootstrap-vcpkg.bat
set VCPKG_ROOT=%USERPROFILE%\vcpkg

cmake --preset uni-debug
cmake --build --preset uni-debug
build\uni-debug\vulkan-renderer.exe
```

> vcpkg caches compiled binaries under `%LOCALAPPDATA%\vcpkg\archives`. Subsequent builds on the same machine are significantly faster.

---

## CMake Presets

| Preset | Platform | Generator | Config | ASan + UBSan |
|--------|----------|-----------|--------|--------------|
| `linux-debug` | Arch Linux | Ninja | Debug | ✅ Enabled |
| `linux-release` | Arch Linux | Ninja | Release | ❌ |
| `uni-debug` | Windows (DMU) | Visual Studio 17 2022 | Debug | ✅ Enabled |
| `uni-release` | Windows (DMU) | Visual Studio 17 2022 | Release | ❌ |

---

## API Documentation (Doxygen)

All public APIs carry Doxygen Javadoc-style headers (`@file`, `@brief`, `@param`, `@return`, `@note`). The generated HTML reference is built on demand and is **not committed** to the repository.

### Build

```bash
# Linux
cmake --build --preset linux-debug --target docs
xdg-open docs/doxygen/html/index.html

# Windows
cmake --build --preset uni-debug --target docs
start docs\doxygen\html\index.html
```

### Prerequisites

```bash
# Linux (Arch)
sudo pacman -S doxygen graphviz

# Windows — download from doxygen.nl and graphviz.org, add both to PATH
```

> The `docs/doxygen/` directory is gitignored. Always regenerate locally. Output is at `docs/doxygen/html/index.html`.

---

## Project Layout

```
FYP-Vulkan-Renderer/
├── CLAUDE.md                        # Claude Code project specification
├── CMakeLists.txt
├── CMakePresets.json
├── vcpkg.json                       # vcpkg dependency manifest
├── vcpkg-configuration.json
│
├── src/
│   ├── main.cpp
│   ├── VulkanContext.cpp/.h         # Instance, device, queues (vk-bootstrap)
│   ├── SwapChain.cpp/.h             # Swapchain, image views, resize
│   ├── Pipeline.cpp/.h             # SPIR-V loading, Dynamic Rendering pipeline
│   ├── Renderer.cpp/.h             # Per-frame command recording + present
│   ├── Mesh.cpp/.h                 # Vertex/index buffers (VMA)
│   ├── Material.cpp/.h             # PBR UBO, descriptor sets, textures
│   └── GaussianSplat.cpp/.h        # M6 — .ply loader, GPU splat buffers
│
├── shaders/
│   ├── triangle.vert / .frag       # M1 baseline
│   ├── mesh.vert                   # M2–M3 geometry + UVs
│   ├── pbr.frag                    # M4 Cook–Torrance BRDF
│   ├── splat.vert / .frag          # M6 Gaussian projection + compositing
│   └── *.spv                       # compiled SPIR-V (gitignored — built by CMake)
│
├── assets/
│   ├── models/                     # OBJ meshes + textures (M3)
│   └── splats/                     # .ply Gaussian splat files (M6)
│
├── docs/
│   ├── Doxyfile                    # Doxygen configuration
│   ├── doxygen/                    # Generated HTML reference (gitignored)
│   └── FYP-Vault/                  # Obsidian knowledge vault
│       ├── Home.md                 # Vault homepage
│       ├── 00-Inbox/               # Unprocessed notes
│       ├── 01-Plans/               # Implementation plans
│       ├── 02-Dev-Log/             # Daily development logs
│       ├── 03-Learnings/           # Vulkan concept notes
│       ├── 04-Archive/             # Completed milestones
│       ├── Research/               # Gaussian splatting, PBR, neural rendering
│       └── Canvas/                 # Visual planning boards
│
└── .claude/
    └── skills/                     # Obsidian Skills for Claude Code
        ├── obsidian-markdown/
        ├── obsidian-bases/
        └── json-canvas/
```

---

## Technical Report

The report is a **core assessed deliverable**, weighted equally alongside the renderer artefact.

| Section | Topic | Primary Source |
|---------|-------|----------------|
| §1 | Introduction & Motivation | [`02-Dev-Log/2026-03-28`](docs/FYP-Vault/02-Dev-Log/2026-03-28.md) |
| §2 | Architecture Overview | [`CLAUDE.md`](CLAUDE.md) + dev logs |
| §3 | Pipeline Deep-Dive | Code + Doxygen reference |
| §4 | Shading & Lighting Model | [`Research/PBR-Shading`](docs/FYP-Vault/Research/PBR-Shading.md) |
| §5 | Challenges & Solutions | [`02-Dev-Log/`](docs/FYP-Vault/02-Dev-Log/) — running log |
| §6 | Performance Analysis | RenderDoc captures (from M2) |
| §7 | Critical Reflection | [`02-Dev-Log/`](docs/FYP-Vault/02-Dev-Log/) — architecture decisions |
| §8 | Conclusion & Future Work | [`Research/Gaussian-Splatting`](docs/FYP-Vault/Research/Gaussian-Splatting.md) |

Section §5 is maintained as a running log throughout development — 15 minutes after each session recording any validation errors encountered and how they were resolved.

---

## Development Log

A daily record maintained in [`docs/FYP-Vault/02-Dev-Log/`](docs/FYP-Vault/02-Dev-Log/). Each entry captures session metadata, validation layer output, bugs encountered, architecture decisions, and a direct cross-reference to the relevant report sections.

| # | Date | Week | Focus | Entry |
|---|------|------|-------|-------|
| 1 | 28 Mar 2026 | Wk 1 · Day 1 | Project setup — MoSCoW, risk register, repo structure | [→](docs/FYP-Vault/02-Dev-Log/2026-03-28.md) |
| 2 | 29 Mar 2026 | Wk 1 · Day 2 | Doxygen headers across all `src/`; VulkanContext planning | [→](docs/FYP-Vault/02-Dev-Log/2026-03-29.md) |
| 3 | 30 Mar 2026 | Wk 1 · Day 3 | CMakeLists fix; VulkanContext plan; M1 concept learning | [→](docs/FYP-Vault/02-Dev-Log/2026-03-30.md) |

> Dev logs are generated per session via `/devlog` in Claude Code and feed directly into Report §5 (Challenges & Solutions) and §7 (Critical Reflection).

---

## Academic Context

Submitted as the Final Year Project for BSc (Hons) Games Production at **De Montfort University, Leicester**. All work is original, completed under the supervision of **Salim Hashu** and **Dr Conor Fahy**.

This project is conceived as a foundation for planned **PhD research in neural rendering at De Montfort University** (commencing October 2026), with particular focus on differentiable rendering, 3D Gaussian Splatting, and GPU-accelerated synthetic data generation for machine learning pipelines.

**Contact:** [mdeeq0@me.com](mailto:mdeeq0@me.com)

---

<div align="center">

<br />

*Vulkan 1.3 Dynamic Rendering · C++20 RAII · Obsidian Knowledge Vault · Doxygen API Reference*

*No `VkRenderPass`. No `VkFramebuffer`. Ever.*

<br />

</div>
