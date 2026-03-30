<div align="center">

<br />

# 🎮 Vulkan Renderer — C++20

<br />

*BSc (Hons) Games Production · Final Year Project*
*De Montfort University, Leicester · 2025–2026*

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

> *A real-time Vulkan 1.3 renderer built from scratch in C++20 —*
> *Dynamic Rendering, explicit GPU synchronisation, and physically based shading,*
> *with a stretch goal of 3D Gaussian Splatting as a pilot for PhD research in neural rendering.*

<br />

<!-- Add a hero GIF or screenshot here once M1 triangle is working -->
<!-- <img src="docs/screenshots/hero.gif" alt="Renderer Preview" style="width:100%" /> -->

</div>

---

## Table of Contents
1. [About This Project](#about-this-project)
2. [Milestones & Progress](#milestones--progress)
   - [M1 — Baseline Vulkan Pipeline + Triangle](#m1--baseline-vulkan-pipeline--triangle)
   - [M2 — Rotating 3D Cube](#m2--rotating-3d-cube)
   - [M3 — OBJ Loading + Texture Mapping](#m3--obj-loading--texture-mapping)
   - [M4 — Renderer Polish + Full Report](#m4--renderer-polish--full-report)
   - [M5 — PBR Shading](#m5--pbr-shading-could-have)
   - [M6 — 3D Gaussian Splatting](#m6--3d-gaussian-splatting-could-have--phd-pilot)
3. [Getting Started](#getting-started)
   - [Linux (Arch)](#linux-arch)
   - [Windows (DMU Lab)](#windows-dmu-lab--no-admin-required)
4. [Project Layout](#project-layout)
5. [API Documentation (Doxygen)](#api-documentation-doxygen)
6. [Coding Standards](#coding-standards)
7. [Key Design Decisions](#key-design-decisions)
8. [Dependencies](#dependencies)
9. [Academic Context](#academic-context)

---

## About This Project

I'm a final-year Games Production student at De Montfort University, and this is my Final Year Project — a real-time 3D renderer built against the Vulkan 1.3 API in C++20.

Rather than following a tutorial end-to-end, I wanted to genuinely understand how GPUs process geometry, how synchronisation barriers work, and how physically based shading mathematics translates into shader code. Every design decision in this project has been made deliberately, and the technical report documents the reasoning behind each one.

This project also serves as a direct foundation for my planned **PhD in neural rendering at DMU (October 2026)** — the Gaussian Splatting milestone is a deliberate first step into that research direction.

**What this is:** A renderer. Scoped carefully — no physics, no ECS, no scene graph, no audio.

**What makes it interesting:** Built entirely around Vulkan 1.3 Dynamic Rendering — no `VkRenderPass`, no `VkFramebuffer`, ever. All GPU memory through VMA. All image transitions through `synchronization2`. Vulkan boilerplate handled by vk-bootstrap so the focus stays on rendering logic.

📋 **Full documentation, milestones, architecture, dev logs, and research notes live in the [Project Vault →](docs/FYP-Vault/Home.md)**

---

## Milestones & Progress

Each milestone builds on the last. Screenshots and notes are added here as each milestone is completed.

### M1 — Baseline Vulkan Pipeline + Triangle
*(Week 2 · Must Have)*

> Coloured triangle via Dynamic Rendering, vk-bootstrap initialisation, SPIR-V shaders compiled by CMake, manual `synchronization2` image layout transitions, validation layers clean on startup and shutdown.

<!-- <img src="docs/screenshots/m1-triangle.png" alt="M1 Triangle" style="width:100%" /> -->
*Screenshot will be added on completion.*

---

### M2 — Rotating 3D Cube
*(Week 3 · Must Have)*

> Vertex/index buffers, staging buffer uploads, VMA depth image, UBO + descriptor sets, MVP transforms, perspective projection.

<!-- <img src="docs/screenshots/m2-cube.png" alt="M2 Rotating Cube" style="width:100%" /> -->
*Screenshot will be added on completion.*

---

### M3 — OBJ Loading + Texture Mapping
*(Week 5 · Must Have)*

> tinyobjloader mesh loading, combined image sampler, `synchronization2` barriers for texture transitions, mipmap generation via `vkCmdBlitImage`.

<!-- <img src="docs/screenshots/m3-obj.png" alt="M3 OBJ + Textures" style="width:100%" /> -->
*Screenshot will be added on completion.*

---

### M4 — Renderer Polish + Full Report
*(Week 9 · Must Have)*

> Interactive camera, swapchain resize handling, zero validation layer errors, reproducible CMake build, complete 8-section technical report submitted.

---

### M5 — PBR Shading *(Could Have)*
*(Week 7)*

> Cook–Torrance BRDF, GGX normal distribution function, Smith geometry term, Schlick Fresnel approximation, Dear ImGui material parameter panel.

<!-- <img src="docs/screenshots/m5-pbr.png" alt="M5 PBR" style="width:100%" /> -->
*Screenshot will be added on completion.*

---

### M6 — 3D Gaussian Splatting *(Could Have · PhD Pilot)*
*(Post M3)*

> `.ply` file ingestion, GPU splat storage buffers, depth sorting, ellipse projection to screen space, alpha compositing. Directly bridges into PhD research in neural rendering.

<!-- <img src="docs/screenshots/m6-splats.png" alt="M6 Gaussian Splatting" style="width:100%" /> -->
*Screenshot will be added on completion.*

---

## Getting Started

Requires a **Vulkan 1.3-capable GPU**, a C++20 toolchain (GCC 12+ / MSVC 2022 / Clang 16+), and CMake 3.25+.

```bash
git clone https://github.com/Raiju-Deeq/FYP-Vulkan-Renderer.git
cd FYP-Vulkan-Renderer
```

### Linux (Arch)

```bash
# Install system dependencies
sudo pacman -S vulkan-radeon vulkan-validation-layers cmake ninja git \
               base-devel autoconf autoconf-archive automake libtool

# Set up vcpkg
git clone https://github.com/microsoft/vcpkg "$HOME/vcpkg"
"$HOME/vcpkg/bootstrap-vcpkg.sh"
export VCPKG_ROOT="$HOME/vcpkg"

# Configure + build
cmake --preset linux-debug
cmake --build --preset linux-debug

# Run with validation layers (always use during development)
VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation ./build/linux-debug/vulkan-renderer
```

### Windows (DMU Lab — No Admin Required)

```bat
git clone https://github.com/microsoft/vcpkg %USERPROFILE%\vcpkg
%USERPROFILE%\vcpkg\bootstrap-vcpkg.bat
set VCPKG_ROOT=%USERPROFILE%\vcpkg

cmake --preset uni-debug
cmake --build --preset uni-debug
build\uni-debug\vulkan-renderer.exe
```

### Build Presets

| Preset | Platform | Config | ASan + UBSan |
|--------|----------|--------|--------------|
| `linux-debug` | Arch Linux | Debug | ✅ |
| `linux-release` | Arch Linux | Release | ❌ |
| `uni-debug` | Windows (DMU) | Debug | ✅ |
| `uni-release` | Windows (DMU) | Release | ❌ |

---

## Project Layout

```
FYP-Vulkan-Renderer/
├── src/                    # C++20 source
│   ├── VulkanContext       #   Instance, device, queues (vk-bootstrap)
│   ├── SwapChain           #   Swapchain + image views
│   ├── Pipeline            #   SPIR-V loading, VkPipeline
│   ├── Renderer            #   Per-frame loop, command buffers
│   ├── Mesh                #   Vertex/index buffers (VMA)
│   ├── Material            #   PBR UBO, descriptor sets, textures
│   └── GaussianSplat       #   GPU splat buffers (M6 stretch)
├── shaders/                # GLSL source → compiled to .spv by CMake (gitignored)
│   ├── triangle.vert/frag  #   M1
│   ├── mesh.vert           #   M2–M4
│   ├── pbr.frag            #   M5
│   └── splat.vert/frag     #   M6
├── assets/
│   ├── models/             # OBJ meshes + textures (M3)
│   └── splats/             # .ply Gaussian splat files (M6)
├── docs/
│   ├── Doxyfile
│   ├── screenshots/        # Milestone evidence images
│   ├── doxygen/            # Generated HTML — gitignored, build locally
│   └── FYP-Vault/          # Obsidian knowledge vault
│       ├── Home.md         # ← start here
│       ├── 01-Plans/
│       ├── 02-Dev-Log/
│       ├── 03-Learnings/
│       └── Research/
├── .claude/skills/         # Claude Code Obsidian skills
├── CLAUDE.md               # Claude Code project specification
├── CMakeLists.txt
└── vcpkg.json
```

---

## API Documentation (Doxygen)

All public APIs carry Doxygen `///` headers. HTML reference is generated locally — not committed to the repo.

```bash
# Linux
cmake --build --preset linux-debug --target docs
xdg-open docs/doxygen/html/index.html

# Windows
cmake --build --preset uni-debug --target docs
start docs\doxygen\html\index.html
```

Prerequisites: `sudo pacman -S doxygen graphviz` on Linux · [doxygen.nl](https://www.doxygen.nl) + [graphviz.org](https://graphviz.org) on Windows.

---

## Coding Standards

All C++20 source follows these conventions throughout:

- **RAII everywhere** — all Vulkan handles wrapped in classes with destructors; no raw `new`/`delete`
- **Doxygen `///` on all public APIs** — `@file`, `@brief`, `@param`, `@return`, `@note`
- **Dynamic Rendering only** — `vkCmdBeginRendering`/`vkCmdEndRendering` exclusively; `VkRenderPass` and `VkFramebuffer` do not exist in this codebase
- **Explicit synchronisation** — all image transitions via `VkImageMemoryBarrier2` + `synchronization2`; no hidden implicit state
- **VMA for all GPU memory** — no direct `vkAllocateMemory` calls anywhere
- **Validation layers on at all times** — zero errors on startup, runtime, and shutdown is a hard requirement
- **GLM configured globally** — `GLM_FORCE_DEPTH_ZERO_TO_ONE`, `GLM_FORCE_RADIANS`, `GLM_ENABLE_EXPERIMENTAL`

---

## Key Design Decisions

| Decision | Reasoning |
|----------|-----------|
| **Vulkan 1.3 Dynamic Rendering only** | Eliminates `VkRenderPass`/`VkFramebuffer` boilerplate; the modern baseline for new renderers and directly compatible with the PhD research direction |
| **vk-bootstrap for initialisation** | Abstracts instance/device/swapchain setup so implementation focus stays on rendering logic, not boilerplate |
| **VMA for all GPU memory** | Industry-standard allocator; avoids per-allocation overhead and aligns with professional practice |
| **synchronization2 for all barriers** | Explicit, readable, and future-proof; required for correct Dynamic Rendering image transitions |
| **Double-buffered frames (`MAX_FRAMES_IN_FLIGHT = 2`)** | Prevents CPU stalling on GPU; per-frame semaphores + fences throttle the pipeline correctly |
| **vcpkg manifest mode** | Reproducible cross-platform builds — all dependencies pinned in `vcpkg.json`, no manual installs |

---

## Dependencies

All managed automatically via **vcpkg manifest mode** (`vcpkg.json`) — no manual installation needed beyond vcpkg itself.

| Library | Purpose |
|---------|---------|
| Vulkan 1.3 SDK | Core graphics API |
| vk-bootstrap | Instance / device / swapchain initialisation |
| GLFW3 | Window creation + input handling |
| GLM | Maths — vectors, matrices, transforms |
| VulkanMemoryAllocator | GPU memory management |
| stb_image | Texture loading |
| tinyobjloader | OBJ mesh loading (M3) |
| tinyply | `.ply` Gaussian splat loading (M6) |
| Dear ImGui | Runtime UI overlay |
| spdlog | Structured logging |

---

## Academic Context

Submitted for **BSc (Hons) Games Production** at De Montfort University, Leicester.
Supervised by **Salim Hashu** and **Dr Conor Fahy**.
Conceived as a foundation for **PhD research in neural rendering at DMU — October 2026**.

**Contact:** [mdeeq0@me.com](mailto:mdeeq0@me.com)

---

<div align="center">

<br />

*No `VkRenderPass`. No `VkFramebuffer`. Ever.*

<br />

</div>
