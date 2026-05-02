![CI](https://github.com/Raiju-Deeq/FYP-Vulkan-Renderer/actions/workflows/build.yml/badge.svg)

# Vulkan 1.3 Renderer

A focused real-time Vulkan 1.3 renderer built in C++20, using vk-bootstrap for setup and Dynamic Rendering throughout.

The project is intentionally small in scope:

- no `VkRenderPass`
- no `VkFramebuffer`
- explicit synchronisation through `synchronization2`
- one textured OBJ model
- simple camera movement
- basic lighting
- resize-safe swapchain handling
- cross-platform builds on Windows and Linux

This repository contains the implementation for my Final Year Project at De Montfort University and supports my longer-term interest in neural rendering.

[![C++](https://img.shields.io/badge/C%2B%2B-20-00599C?style=flat-square&logo=cplusplus&logoColor=white)](https://en.cppreference.com/w/cpp/20)
[![Vulkan](https://img.shields.io/badge/Vulkan-1.3-AC162C?style=flat-square&logo=vulkan&logoColor=white)](https://registry.khronos.org/vulkan/)
[![CMake](https://img.shields.io/badge/CMake-3.25+-064F8C?style=flat-square&logo=cmake&logoColor=white)](https://cmake.org/)
[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20Windows-lightgrey?style=flat-square&logo=linux&logoColor=white)](README.md#getting-started)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow?style=flat-square)](LICENSE)

---

## Project Context

**Programme:** BSc (Hons) Games Production, De Montfort University, Leicester  
**Author:** Mohamed Deeq Mohamed (P2884884)  
**Supervisor:** Salim Hasshu  
**Focus:** Vulkan rendering fundamentals, explicit GPU programming, and technical reflection

The project is designed to stay practical and achievable within a short development window. The goal is to produce a renderer that is stable, explainable, and well-documented rather than to build a full engine.

For more information on the project, please see [FYP-Vulkan-Renderer Docs](https://github.com/Raiju-Deeq/FYP-Vulkan-Renderer/tree/main/docs/FYP-Vault)


---

## Core Scope

The core project must include the following:

- [ ] Window creation
- [ ] Vulkan setup
- [ ] Dynamic Rendering
- [ ] Model loading
- [ ] Camera
- [ ] Lighting
- [ ] Report and evaluation

These are the only features that must be complete for the project to count as successful.

---

## Should Have

These are high-priority additions targeted after the core is working.

- [ ] Resize-safe swapchain handling
  - Recreate the swapchain cleanly after window resizes.
- [ ] Wireframe or debug normals toggle
  - Add a simple visual debugging mode to help inspect the mesh and explain the pipeline.

## Stretch Goals

Stretch work is only attempted if the core renderer and report are already stable.

- [ ] PBR
  - Replace the basic lighting model with a simple physically based shading pass.
- [ ] Mipmaps
  - Generate mipmaps for the loaded texture.
- [ ] Gaussian Splat Rendering
  - Ingest a `.ply` file and render Gaussian splats as a final stretch objective.

Gaussian splatting is deliberately placed last so it does not compete with the core renderer, the report, or the testing work.

---

## Features

> **Note:** The project is organised by priority rather than by engine-style systems.

- [ ] **Minimal Vulkan 1.3 pipeline** · vk-bootstrap initialisation, Dynamic Rendering, validation layers, SPIR-V shaders
- [ ] **One textured OBJ model** · tinyobjloader, buffer uploads, texture sampling
- [ ] **Simple camera control** · orbit or free movement for inspection
- [ ] **Basic lighting** · enough to make the mesh readable and demonstrable
- [ ] **Technical report and evaluation** · implementation rationale, testing, reflection, and limitations
- [ ] **Should Have** · resize-safe swapchain, wireframe or debug normals toggle
- [ ] **Could Have (stretch)** · PBR, mipmaps, Gaussian splats — only after core is complete

---

## Screenshots

<p align="center">
  <img src="docs/screenshots/M1-triangle.png" alt="Initial Vulkan Dynamic Rendering test" width="720"/>
  <br/>
  <em>Initial Vulkan 1.3 test using Dynamic Rendering</em>
</p>


---

## Key Design Decisions

| Decision                           | Why                                                          |
| ---------------------------------- | ------------------------------------------------------------ |
| **vk-bootstrap**                   | Reduces Vulkan setup boilerplate so the project can focus on rendering logic. |
| **Dynamic Rendering only**         | Removes render pass and framebuffer boilerplate from the renderer. |
| **Explicit synchronisation**       | Keeps image transitions and frame flow clear and explainable. |
| **One mesh first**                 | Keeps scope manageable and lets the report focus on correctness and understanding. |
| **Resize-safe swapchain handling** | Prevents the renderer from breaking when the window changes size. |
| **Cross-platform build**           | Ensures the project runs on both Linux and Windows using the same codebase. |

---

## Getting Started

**Requirements:** Vulkan 1.3 GPU, C++20 compiler, CMake 3.25+, and vcpkg or an equivalent dependency setup.

```bash
git clone https://github.com/Raiju-Deeq/FYP-Vulkan-Renderer.git
cd FYP-Vulkan-Renderer
```

### Linux

```bash
sudo pacman -S vulkan-radeon vulkan-validation-layers cmake ninja git base-devel

git clone https://github.com/microsoft/vcpkg "$HOME/vcpkg"
"$HOME/vcpkg/bootstrap-vcpkg.sh"

export VCPKG_ROOT="$HOME/vcpkg"

cmake --preset linux-debug
cmake --build --preset linux-debug
VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation ./build/linux-debug/vulkan-renderer
```

### Windows

```bat
# 1. Clone vcpkg
git clone https://github.com/microsoft/vcpkg "$env:USERPROFILE\vcpkg"

# 2. Run the bootstrap script
& "$env:USERPROFILE\vcpkg\bootstrap-vcpkg.bat"

# 3. Set the VCPKG_ROOT environment variable
$env:VCPKG_ROOT = "$env:USERPROFILE\vcpkg"

# 4. (Recommended) Also add vcpkg to your PATH for the session
$env:PATH = "$env:VCPKG_ROOT;$env:PATH"
```

---

## Build Presets

| Preset          | Platform | Config  | Notes              |
| --------------- | -------- | ------- | ------------------ |
| `linux-debug`   | Linux    | Debug   | Validation enabled |
| `linux-release` | Linux    | Release | Optimised build    |
| `uni-debug`     | Windows  | Debug   | Validation enabled |
| `uni-release`   | Windows  | Release | Optimised build    |

---

## Architecture

### Project Structure

```text
FYP-Vulkan-Renderer/
├── CMakeLists.txt
├── CMakePresets.json
├── vcpkg.json
├── vcpkg-configuration.json
├── README.md
├── LICENSE
├── Doxyfile
├── src/
│   ├── main.cpp
│   ├── application.hpp / application.cpp
│   ├── window.hpp / window.cpp
│   ├── vulkan_context.hpp / vulkan_context.cpp
│   ├── swapchain.hpp / swapchain.cpp
│   ├── frame_data.hpp / frame_data.cpp
│   ├── graphics_pipeline.hpp / graphics_pipeline.cpp
│   ├── gpu_buffer.hpp / gpu_buffer.cpp
│   ├── mesh.hpp / mesh.cpp
│   └── texture.hpp / texture.cpp
├── shaders/
│   ├── triangle.vert
│   ├── triangle.frag
│   ├── mesh.vert
│   └── mesh.frag
├── assets/
│   ├── models/
│   └── textures/
├── docs/
│   ├── dev-log/
│   │   └── gaussian-splat-stretch-goal.md
│   ├── prototype-evidence/
│   │   └── M1-triangle.png
│   └── renderdoc-captures/
```

Compiled shader outputs (`*.spv`), build directories, and generated Doxygen
HTML under `docs/doxygen/` are ignored and regenerated locally.

---

## Dependencies

All dependencies are managed through vcpkg manifest mode.

| Library                                                      | Purpose                                     |
| ------------------------------------------------------------ | ------------------------------------------- |
| [Vulkan 1.3 SDK](https://vulkan.lunarg.com/)                 | Core graphics API                           |
| [vk-bootstrap](https://github.com/charles-lunarg/vk-bootstrap) | Instance, device, and swapchain setup       |
| [GLFW](https://www.glfw.org/)                                | Window creation and input                   |
| [GLM](https://github.com/g-truc/glm)                         | Math                                        |
| [tinyobjloader](https://github.com/tinyobjloader/tinyobjloader) | OBJ model loading                           |
| [stb_image](https://github.com/nothings/stb)                 | Material loading                             |
| [spdlog](https://github.com/gabime/spdlog)                   | Logging                                     |
| [Dear ImGui](https://github.com/ocornut/imgui)               | Optional debug UI                           |
| [tinyply](https://github.com/ddiakopoulos/tinyply)           | Optional `.ply` loading for Gaussian splats |

---

## Coding Standards

- **C++20 RAII**: prefer scoped ownership and clean teardown.
- **Dynamic Rendering only**: no `VkRenderPass` and no `VkFramebuffer`.
- **Explicit synchronisation**: use `VkImageMemoryBarrier2` and `synchronization2`.
- **Validation layers on during development**: aim for zero validation errors in the final build.
- **Doxygen on public APIs**: document the main classes and renderer functions clearly.
- **Keep the scope small**: core first, stretch features only after the base renderer is stable.

---

## Report Focus

The report will concentrate on:

- why Dynamic Rendering was chosen
- how vk-bootstrap simplified initialisation
- how the swapchain is handled safely on resize
- how the OBJ model is loaded and drawn
- how the camera and lighting support the final image
- what was tested, what failed, and what was learned
- why the stretch goals were kept separate from the core

---

## References

- [Vulkan 1.3 Specification](https://registry.khronos.org/vulkan/specs/1.3/html/)
- [vk-bootstrap](https://github.com/charles-lunarg/vk-bootstrap)
- [vkguide.dev](https://vkguide.dev/)
- [tinyobjloader](https://github.com/tinyobjloader/tinyobjloader)
- [LearnOpenGL: PBR Theory](https://learnopengl.com/PBR/Theory)
- [3D Gaussian Splatting (Kerbl et al., 2023)](https://repo-sam.inria.fr/fungraph/3d-gaussian-splatting/)

---

## License

[MIT](LICENSE)
