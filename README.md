<div align="center">

# Vulkan Renderer — C++20

BSc (Hons) Games Production · Final Year Project
De Montfort University · 2026
Mohamed Deeq Mohamed · P2884884

*Supervisors: Salim Hashu · Dr Conor Fahy*

![C++](https://img.shields.io/badge/C%2B%2B-20-blue?style=flat-square&logo=cplusplus)
![Vulkan](https://img.shields.io/badge/Vulkan-1.3-red?style=flat-square&logo=vulkan)
![CMake](https://img.shields.io/badge/CMake-3.25%2B-064F8C?style=flat-square&logo=cmake)
![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux-lightgrey?style=flat-square)
![License](https://img.shields.io/badge/License-Academic-green?style=flat-square)

</div>

---

## Overview

This repository contains my final year project: a real-time 3D renderer built from scratch using the **Vulkan 1.3 graphics API** and **C++20**.

The renderer is built around **Vulkan 1.3 Dynamic Rendering** (`VK_KHR_dynamic_rendering`, promoted to core in 1.3). That means no traditional render passes or framebuffers at all — all colour and depth attachments are defined inline when recording commands, and image layout transitions are handled explicitly via `synchronization2` barriers.

The learning path for the project is:

> Baseline Pipeline → 3D Geometry → GPU Memory → Texture Mapping → Physically-Based Shading

On the academic side this is my BSc Games Production FYP; on the research side it acts as a **pilot for future work in neural rendering and AI‑driven graphics**, and is designed so that ray tracing (`VK_KHR_ray_tracing_pipeline`) and GPU‑accelerated synthetic data generation can be layered in later.

---

## Tech Stack

| Tool / Library | Version | Purpose |
|----------------|---------|---------|
| **Vulkan SDK** (LunarG) | 1.3 | Core graphics API |
| **C++20** | MSVC / GCC / Clang | Implementation language |
| **vk-bootstrap** | latest | Vulkan instance, device, and swapchain initialisation |
| **GLFW** | 3 | Window creation and input |
| **GLM** | latest | Maths library (vectors, matrices) |
| **Vulkan Memory Allocator (VMA)** | latest | GPU memory management |
| **stb_image** | latest | Texture / image loading |
| **tinyobjloader** | latest | OBJ mesh loading |
| **Dear ImGui** | latest | Runtime UI for materials / lighting |
| **spdlog** | latest | Logging |
| **shaderc / glslc** | latest | GLSL → SPIR‑V compilation |

All dependencies are managed via **vcpkg** in manifest mode and are pulled in automatically on the first CMake configure.

> **Why vk-bootstrap?**  
> Vulkan instance creation, physical device selection, and queue family setup are necessary but low-value boilerplate — the kind of code that every professional Vulkan project abstracts away. vk-bootstrap handles that cleanly so development effort can go toward the parts that actually matter: the Dynamic Rendering pipeline, synchronisation model, VMA memory strategy, and PBR shading. The technical report covers the underlying initialisation sequence in full so the understanding is demonstrated regardless.

---

## Getting Started

The project is developed and tested on **Arch Linux** and **Windows (DMU lab machines)**, but should work on any platform with a Vulkan 1.3‑capable GPU and a C++20 toolchain.

### 1. Clone the repo

```bash
git clone https://github.com/Raiju-Deeq/FYP-Vulkan-Renderer.git
cd FYP-Vulkan-Renderer
```

### 2. Linux (Arch) setup

**Prerequisites:**

```bash
sudo pacman -S vulkan-radeon vulkan-validation-layers cmake ninja git base-devel \
               autoconf autoconf-archive automake libtool
```

**Configure and build:**

```bash
# Clone vcpkg if you don't already have it
git clone https://github.com/microsoft/vcpkg "$HOME/vcpkg"
"$HOME/vcpkg/bootstrap-vcpkg.sh"
export VCPKG_ROOT="$HOME/vcpkg"

# Configure (first run will build all dependencies: ~10–20 minutes)
cmake --preset linux-debug

# Build
cmake --build --preset linux-debug

# Run
./build/linux-debug/vulkan-renderer
```

### 3. Windows (DMU lab PCs, no admin required)

> First‑time setup only. Run in a normal **Developer Command Prompt** or **cmd.exe**.  
> vcpkg caches binaries under `%LOCALAPPDATA%\vcpkg\archives` so subsequent builds on the same machine are much faster.

```bat
REM --- vcpkg setup (once per machine) ---
git clone https://github.com/microsoft/vcpkg %USERPROFILE%\vcpkg
%USERPROFILE%\vcpkg\bootstrap-vcpkg.bat

REM --- Set VCPKG_ROOT for this session ---
set VCPKG_ROOT=%USERPROFILE%\vcpkg

REM --- Clone the project ---
cd %USERPROFILE%\Documents
git clone https://github.com/Raiju-Deeq/FYP-Vulkan-Renderer.git
cd FYP-Vulkan-Renderer

REM --- Configure (first run installs all dependencies) ---
cmake --preset uni-debug

REM --- Build ---
cmake --build --preset uni-debug
```

---

## CMake Presets

| Preset | Platform | Generator | Config |
|--------|----------|-----------|--------|
| `linux-debug` | Arch Linux | Ninja | Debug |
| `linux-release` | Arch Linux | Ninja | Release |
| `uni-debug` | Windows (DMU) | Visual Studio 17 2022 | Debug |
| `uni-release` | Windows (DMU) | Visual Studio 17 2022 | Release |

---

## Project Layout

```text
FYP-Vulkan-Renderer/
├── CMakeLists.txt
├── CMakePresets.json
├── vcpkg.json                  # dependency manifest
├── vcpkg-configuration.json    # pinned vcpkg baseline
├── src/
│   ├── main.cpp
│   ├── VulkanContext.cpp/.h    # instance, device, surface, queues (via vk-bootstrap)
│   ├── SwapChain.cpp/.h        # swapchain (via vk-bootstrap), image views, resize logic
│   ├── Pipeline.cpp/.h         # dynamic rendering pipeline builder
│   ├── Mesh.cpp/.h             # vertex/index buffers, OBJ loading
│   ├── Material.cpp/.h         # PBR material parameters, descriptors
│   └── Renderer.cpp/.h         # per-frame command recording
├── shaders/
│   ├── triangle.vert           # M1 baseline vertex shader
│   ├── triangle.frag           # M1 baseline fragment shader
│   ├── mesh.vert               # M2–M3 geometry + UVs
│   └── pbr.frag                # M4 Cook–Torrance BRDF
├── assets/
│   └── models/                 # OBJ + texture assets
└── third_party/
```

---

## Architecture

**vk-bootstrap** handles instance creation, physical device selection, logical device setup, and swapchain construction. Everything from the first `vkCmd*` call onwards is written directly against the Vulkan API with no further abstractions — that's where the real learning happens and where the project spends most of its effort.

The remaining architectural decisions:

- **Dynamic Rendering only**  
  All rendering uses `vkCmdBeginRendering` / `vkCmdEndRendering`. There are no `VkRenderPass` or `VkFramebuffer` objects anywhere in the codebase.

- **Explicit synchronisation**  
  Image layout transitions are handled with `VkImageMemoryBarrier2` and `synchronization2`. The goal is to understand and own the synchronisation model, not hide it.

- **VMA‑backed memory**  
  All GPU allocations (vertex/index/uniform buffers, textures, depth image) go through **Vulkan Memory Allocator**. Consistent, debuggable, and production-representative.

- **RAII C++20**  
  Vulkan handles are wrapped in RAII types throughout. Ownership is explicit and shutdown is clean — zero validation errors on exit is a hard requirement at every milestone.

- **ImGui as a first‑class tool**  
  Dear ImGui is integrated via a second dynamic rendering pass using `VK_ATTACHMENT_LOAD_OP_LOAD`, giving live runtime control over material and lighting parameters from M4 onwards.

---

## Roadmap & Milestones

| Milestone | Description | Grade Band | Target | Status |
|----------|-------------|-----------|--------|--------|
| **M1** | Baseline Vulkan setup and triangle render | Third (40%+) | Week 2 — 13 Apr | ⏳ Pending |
| **M2** | Rotating 3D cube | 2:2 (50%+) | Week 3 — 20 Apr | ⏳ Pending |
| **M3** | OBJ loading and texture mapping | 2:1 (60%+) | Week 5 — 4 May | ⏳ Pending |
| **M4** | PBR shading and lighting | First (70%+) | Week 7 — 18 May | ⏳ Pending |
| **M5** | Renderer polish and technical report | First (70%+) | Week 9 — 1 Jun | ⏳ Pending |

> A First‑Class outcome requires **both** M1–M5 artefact completion **and** a high-quality technical report. Neither on its own is enough.

### Milestone details

**M1 — Baseline Vulkan Pipeline**  
vk-bootstrap brings up the instance, physical device, logical device, and swapchain. From there: SPIR‑V shader pipeline, `vkCmdBeginRendering` / `vkCmdEndRendering`, a reusable image layout transition helper, and a coloured triangle on screen.

**M2 — Rotating 3D Cube**  
Staging‑buffer‑backed vertex and index buffers, VMA-managed depth image, and an MVP matrix pushed per‑frame via a uniform buffer and descriptor set.

**M3 — OBJ + Texture**  
`tinyobjloader` for mesh data, combined image sampler in the fragment shader, explicit layout transitions via `synchronization2`, and a mipmap chain generated with `vkCmdBlitImage`.

**M4 — PBR Shading**  
Cook–Torrance BRDF (GGX NDF, Smith G, Schlick Fresnel) plus Lambertian diffuse. Metallic, roughness, and albedo tweakable live via ImGui. Blender Cycles used as a visual reference for validation.

**M5 — Polish + Report**  
Interactive orbit/FPS camera, robust swapchain resize, clean shutdown with zero validation errors, reproducible CMake build on both platforms, and an 8-section technical report.

---

## Technical Report

The report is a **core deliverable** alongside the renderer — equal weight to the artefact for a First.

1. **Introduction & Motivation** — Why Vulkan, why C++20, why Dynamic Rendering, and how this ties into future PhD work in neural rendering
2. **Architecture Overview** — `VulkanContext`, `SwapChain`, `Pipeline`, `Mesh`, `Material`, `Renderer`; role of vk-bootstrap; RAII patterns
3. **Pipeline Deep‑Dive** — Command sequence, dynamic rendering setup, synchronisation primitives, VMA usage
4. **Shading & Lighting Model** — Derivation of Lambertian + Cook–Torrance (GGX, Smith G, Schlick Fresnel)
5. **Challenges & Solutions** — Validation errors, sync bugs, memory issues — diagnosed and fixed *(running log, maintained throughout)*
6. **Performance Analysis** — RenderDoc frame captures, GPU timings per pass, draw call overhead
7. **Critical Reflection** — What works, what doesn't, what would change if starting again
8. **Conclusion & Future Work** — Ray tracing, post‑effects, neural rendering, synthetic data generation

---

## Validation

Validation layers stay **on** for the entire project. A clean run looks like:

```bash
# Linux
VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation ./build/linux-debug/vulkan-renderer
```

Zero validation errors on startup and shutdown is a hard requirement at every milestone checkpoint.

---

## Academic Context

This project is part of my **BSc (Hons) Games Production** degree at De Montfort University, assessed as my Final Year Project. All work is my own, completed under the supervision of **Salim Hashu** and **Dr Conor Fahy**, and is intended for educational and research use.

For questions or enquiries: [mdeeq0@me.com](mailto:mdeeq0@me.com)

---

<div align="center">

*Vulkan 1.3 Dynamic Rendering · No `VkRenderPass`. No `VkFramebuffer`. Ever.*

</div>
