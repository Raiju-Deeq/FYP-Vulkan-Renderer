<div align="center">

# Vulkan Renderer — C++20

**BSc (Hons) Games Production — Final Year Project**
De Montfort University · 2026
Mohamed Deeq Mohamed · P2884884

*Supervisors: Salim Hashu / Dr Conor Fahy*

![C++](https://img.shields.io/badge/C%2B%2B-20-blue?style=flat-square&logo=cplusplus)
![Vulkan](https://img.shields.io/badge/Vulkan-1.3-red?style=flat-square&logo=vulkan)
![CMake](https://img.shields.io/badge/CMake-3.25%2B-064F8C?style=flat-square&logo=cmake)
![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux-lightgrey?style=flat-square)
![License](https://img.shields.io/badge/License-Academic-green?style=flat-square)

</div>

---

## Overview

A real-time 3D renderer built from scratch using the **Vulkan 1.3 graphics API** and **C++20**, developed as a Final Year Project at De Montfort University.

The renderer adopts **Vulkan 1.3 Dynamic Rendering** (`VK_KHR_dynamic_rendering`, promoted to core in Vulkan 1.3) as a deliberate architectural choice — eliminating `VkRenderPass` and `VkFramebuffer` objects entirely in favour of inline attachment descriptions at the point of command recording. No `VkRenderPass` or `VkFramebuffer` objects are created at any point in this project.

Development follows a structured learning progression:

Baseline Pipeline → 3D Geometry → GPU Memory → Texture Mapping → Physically-Based Shading

text

Beyond the undergraduate assessment, this project is conceived as a **pilot study for postgraduate research** in neural rendering and AI-driven graphics. The codebase is deliberately architected to be extensible toward `VK_KHR_ray_tracing_pipeline` and GPU-accelerated synthetic data generation.

---

## Tech Stack

| Tool / Library | Version | Purpose |
|---|---|---|
| **Vulkan SDK** (LunarG) | 1.3 | Core graphics API |
| **C++20** | MSVC / GCC / Clang | Implementation language |
| **GLFW** | 3 | Window creation and input |
| **GLM** | latest | Mathematics library (vectors, matrices) |
| **Vulkan Memory Allocator (VMA)** | latest | GPU memory management |
| **stb_image** | latest | Texture / image loading |
| **tinyobjloader** | latest | OBJ mesh loading |
| **Dear ImGui** | latest | Runtime material and lighting controls overlay |
| **spdlog** | latest | Structured logging |
| **shaderc / glslc** | latest | GLSL → SPIR-V shader compilation |

All dependencies are managed via **vcpkg** in manifest mode and install automatically on first CMake configure.

---

## Building

### Linux (Arch)

**Prerequisites**

```bash
sudo pacman -S vulkan-radeon vulkan-validation-layers cmake ninja git base-devel \
               autoconf autoconf-archive automake libtool
```

**Clone and Build**

```bash
git clone https://github.com/Raiju-Deeq/vulkan-renderer.git
cd vulkan-renderer

# Clone vcpkg if you have not already
git clone https://github.com/microsoft/vcpkg $HOME/vcpkg
$HOME/vcpkg/bootstrap-vcpkg.sh
export VCPKG_ROOT="$HOME/vcpkg"

# Configure — first run compiles all dependencies (~10–20 min)
cmake --preset linux-debug

# Build
cmake --build --preset linux-debug

# Run
./build/linux-debug/vulkan-renderer
```

---

### Windows (DMU University PCs — No Admin Required)

> **First-time setup only.** Run these commands once in a regular Command Prompt.
> The first configure step downloads and compiles all dependencies — allow **15–25 minutes**.
> Subsequent sessions on the same PC are faster as vcpkg caches compiled packages at `%LOCALAPPDATA%\vcpkg\archives\`.
> If the PC has been wiped since your last session, repeat the full setup below.

```bat
REM --- vcpkg setup (once per machine) ---
git clone https://github.com/microsoft/vcpkg %USERPROFILE%\vcpkg
%USERPROFILE%\vcpkg\bootstrap-vcpkg.bat

REM --- Set VCPKG_ROOT for this session ---
set VCPKG_ROOT=%USERPROFILE%\vcpkg

REM --- Clone the project ---
cd %USERPROFILE%\Documents
git clone https://github.com/Raiju-Deeq/vulkan-renderer.git
cd vulkan-renderer

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

## Project Structure

vulkan-renderer/
├── CMakeLists.txt
├── CMakePresets.json
├── vcpkg.json ← dependency manifest
├── vcpkg-configuration.json ← pinned vcpkg baseline
├── src/
│ ├── main.cpp
│ ├── VulkanContext.cpp / .h ← instance, device, surface, queues
│ ├── SwapChain.cpp / .h ← swapchain, image views, resize handling
│ ├── Pipeline.cpp / .h ← dynamic rendering pipeline builder
│ ├── Mesh.cpp / .h ← vertex/index buffers, OBJ loading
│ ├── Material.cpp / .h ← PBR parameters, descriptor sets
│ └── Renderer.cpp / .h ← per-frame command recording
├── shaders/
│ ├── triangle.vert ← M1 baseline vertex shader
│ ├── triangle.frag ← M1 baseline fragment shader
│ ├── mesh.vert ← M2–M3 geometry + UV shader
│ └── pbr.frag ← M4 Cook–Torrance BRDF shader
├── assets/
│ └── models/ ← OBJ + texture assets
└── third_party/

text

---

## Architecture Notes

This renderer uses **Vulkan 1.3 Dynamic Rendering** throughout. The key consequences of this decision are:

- All rendering is initiated via `vkCmdBeginRendering` / `vkCmdEndRendering`
- Colour and depth attachments are described inline via `VkRenderingAttachmentInfo`
- Image layout transitions are performed manually using `VkImageMemoryBarrier2` (`synchronization2`)
- Swapchain recreation on resize only requires teardown of swapchain images, image views, and the depth image — no render pass or framebuffer objects exist to manage
- The Dear ImGui overlay is composited via a second `vkCmdBeginRendering` call using `VK_ATTACHMENT_LOAD_OP_LOAD`

GPU memory is managed entirely through the **Vulkan Memory Allocator (VMA)**. All allocations — vertex buffers, index buffers, uniform buffers, texture images, and the depth image — go through VMA.

---

## Milestone Progress

| Milestone | Description | Grade Band | Target | Status |
|-----------|-------------|-----------|--------|--------|
| **M1** | Baseline Vulkan setup and triangle render | Third (40%+) | Week 2 — 13 Apr | ⏳ Pending |
| **M2** | Rotating 3D cube | 2:2 (50%+) | Week 3 — 20 Apr | ⏳ Pending |
| **M3** | OBJ loading and texture mapping | 2:1 (60%+) | Week 5 — 4 May | ⏳ Pending |
| **M4** | PBR shading and lighting | First (70%+) | Week 7 — 18 May | ⏳ Pending |
| **M5** | Renderer polish and technical report | First (70%+) | Week 9 — 1 Jun | ⏳ Pending |

> A First Class Honours outcome requires **both** M1–M5 software artefact completion **and** a high-quality technical report. Neither alone is sufficient.

### M1 — Baseline Vulkan Pipeline
Vulkan 1.3 environment with Dynamic Rendering. Renders a single coloured triangle.
Key systems: instance creation, physical/logical device selection, surface, swapchain, SPIR-V shader compilation, `vkCmdBeginRendering` / `vkCmdEndRendering`, manual image layout transitions via pipeline barriers.

### M2 — Rotating 3D Cube
Vertex and index buffers populated via a staging buffer upload path into device-local GPU memory. VMA-managed depth image attached dynamically via `VkRenderingInfo::pDepthAttachment`. MVP transformation matrix delivered per-frame through a UBO and descriptor set.

### M3 — OBJ Loading and Texture Mapping
`tinyobjloader` mesh loading. Combined image sampler bound to the fragment shader through an updated descriptor set layout. Explicit image layout transitions using `synchronization2`. Mipmap chain generation via repeated `vkCmdBlitImage` calls.

### M4 — PBR Shading and Lighting
Cook–Torrance specular BRDF: GGX normal distribution function, Smith geometry masking-shadowing term, Schlick Fresnel approximation. Parametrised by metallic and roughness values adjustable at runtime via a Dear ImGui panel. Lambertian diffuse as the minimum acceptable outcome.

### M5 — Renderer Polish and Technical Report
Submission-ready renderer with interactive camera, stable swapchain resize handling, zero validation errors, reproducible CMake build. Technical report: 8 mandatory sections covering architecture, pipeline deep-dive, PBR mathematics, performance analysis (RenderDoc), challenges log, and critical reflection.

---

## Technical Report — Required Sections

The technical report is a **mandatory assessed component** of equal weight to the software artefact.

1. **Introduction and Motivation** — The case for Vulkan over higher-level APIs; C++20; Dynamic Rendering adoption; relevance to postgraduate research
2. **Architecture Overview** — Module diagram: `VulkanContext`, `SwapChain`, `Pipeline`, `Mesh`, `Material`, `Renderer`; RAII patterns; pipeline builder approach
3. **Pipeline Deep-Dive** — Dynamic rendering command sequence, synchronisation primitives (`VkSemaphore`, `VkFence`), VMA memory management strategy
4. **Shading and Lighting Model** — Mathematical derivation of PBR terms: Lambertian diffuse, GGX NDF, Cook–Torrance specular BRDF
5. **Challenges and Solutions** — Running log of validation errors, synchronisation issues, and memory problems encountered and resolved *(maintained incrementally)*
6. **Performance Analysis** — RenderDoc frame capture data: GPU time per `vkCmdBeginRendering` region, draw call overhead, memory allocation profile
7. **Critical Reflection** — Honest assessment of limitations and decisions that would be approached differently in hindsight
8. **Conclusion and Future Work** — Extensions: ray tracing (`VK_KHR_ray_tracing_pipeline`), post-processing passes, neural rendering and GPU-accelerated synthetic data generation

---

## Validation

Vulkan validation layers must remain **active throughout all development**. Every milestone evidence package includes terminal output confirming zero validation errors on both startup and shutdown.

```bash
# Confirm validation layers are active (Linux)
VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation ./build/linux-debug/vulkan-renderer
```

---

## Academic Context

This project is submitted as part of the requirements for the **BSc (Hons) Games Production** degree at De Montfort University. All work is original and completed by **Mohamed Deeq Mohamed (P2884884)** under the supervision of **Salim Hashu** and **Dr Conor Fahy**.

The project adheres to the university's academic integrity policies and is intended for educational and research purposes. It is additionally conceived as a pilot study for planned postgraduate research in neural rendering at De Montfort University.

For questions or enquiries, contact: [mdeeq0@me.com](mailto:mdeeq0@me.com)

---

<div align="center">

*De Montfort University — BSc (Hons) Games Production*
*FYP Contract v7.0 · Vulkan 1.3 Dynamic Rendering · No `VkRenderPass`. No `VkFramebuffer`. Ever.*

</div>
