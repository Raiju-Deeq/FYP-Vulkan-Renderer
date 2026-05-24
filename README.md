![CI](https://github.com/Raiju-Deeq/FYP-Vulkan-Renderer/actions/workflows/build.yml/badge.svg)

# Raiju Renderer

## Product Page

[View the Raiju Renderer product page](https://raiju-deeq.github.io/FYP-Vulkan-Renderer/)

**Release State:** v0.3.0 Gaussian Splat Prototype / Final Year Project
**Renderer Version:** Textured OBJ + Gaussian-style PLY splat vertical slice
**Graphics API:** Vulkan 1.3  
**Language:** C++20  
**Platforms:** Linux and Windows

Raiju Renderer is a focused real-time Vulkan renderer built to make explicit GPU programming easier to inspect, explain, and extend. It is not a full game engine. It is a compact rendering project that shows how a 3D model becomes a visible image through Vulkan initialisation, swapchain management, command recording, GPU memory uploads, synchronisation, Dynamic Rendering, texture sampling, depth testing, and presentation.

The project is currently developed as my Final Year Project at De Montfort University and is also intended to become a public learning resource for people interested in Vulkan, real-time rendering, graphics programming, and eventually neural rendering.

[![C++](https://img.shields.io/badge/C%2B%2B-20-00599C?style=flat-square&logo=cplusplus&logoColor=white)](https://en.cppreference.com/w/cpp/20)
[![Vulkan](https://img.shields.io/badge/Vulkan-1.3-AC162C?style=flat-square&logo=vulkan&logoColor=white)](https://registry.khronos.org/vulkan/)
[![CMake](https://img.shields.io/badge/CMake-3.25+-064F8C?style=flat-square&logo=cmake&logoColor=white)](https://cmake.org/)
[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20Windows-lightgrey?style=flat-square&logo=linux&logoColor=white)](https://github.com/Raiju-Deeq/FYP-Vulkan-Renderer#building-for-windows-and-linux)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow?style=flat-square)](LICENSE)

---

## Screenshots

<p align="center">
  <img src="docs/prototype-evidence/M1-triangle.png" alt="Milestone 1 Vulkan Dynamic Rendering triangle" width="780"/>
  <br/>
  <em>Milestone 1: first Vulkan 1.3 render path using Dynamic Rendering.</em>
</p>

<p align="center">
  <img src="docs/prototype-evidence/M2-textured-obj.png" alt="Milestone 2 textured OBJ model rendered in Vulkan" width="780"/>
  <br/>
  <em>Milestone 2: textured OBJ model rendered with vertex/index buffers, texture sampling, depth testing, and debug UI.</em>
</p>

<p align="center">
  <img src="docs/prototype-evidence/MilestoneUpdate.png" alt="Updated renderer milestone view with runtime controls" width="780"/>
  <br/>
  <em>Milestone update: current renderer state showing the textured model workflow with the live ImGui inspection controls.</em>
</p>

<p align="center">
  <img src="docs/prototype-evidence/MilestoneUpdate-GaussianSplats.png" alt="Gaussian splat renderer milestone update with ImGui controls" width="780"/>
  <br/>
  <em>Milestone update: experimental Gaussian-style PLY splat rendering shown through the live ImGui inspection controls.</em>
</p>

---

## Purpose

This project was created to learn and demonstrate low-level real-time rendering without hiding the important parts inside a large engine.

Modern engines such as Unreal and Unity are excellent production tools, but they abstract away the details that renderer programmers need to understand. Raiju Renderer keeps those details visible:

- how a Vulkan instance, device, queue, and surface are created;
- how swapchain images are acquired, rendered into, and presented;
- how command buffers are recorded and submitted;
- how GPU buffers and images are allocated and uploaded;
- how image layouts and synchronisation are managed;
- how shaders, descriptors, push constants, and pipelines connect;
- how resize and minimise events are handled safely.

The aim is to build a renderer that is small enough to understand, but real enough to provide useful evidence of Vulkan rendering knowledge.

---

## Use Cases

Raiju Renderer currently has three main use cases.

1. **Learning resource**  
   The codebase is designed to be readable and explainable for students or developers who want to understand Vulkan fundamentals.

2. **Renderer prototype**  
   The project demonstrates a complete rendering vertical slice: assets on disk become GPU resources, GPU commands draw a frame, and the result is presented to the screen.

3. **Foundation for future graphics research**  
   The renderer is intentionally scoped so that later experiments, such as PBR, better scene loading, or Gaussian splatting, can be added after the core Vulkan path is stable.

---

## Current Supported Features

### Graphics

- Vulkan 1.3 renderer
- Dynamic Rendering path
- No legacy `VkRenderPass`
- No legacy `VkFramebuffer`
- Depth attachment support
- Textured OBJ rendering
- Cook-Torrance direct PBR lighting
- Mipmap generation for loaded textures
- Optional PBR texture slots for base colour, metallic/roughness, AO, emissive, and reserved normal maps
- Experimental ASCII/binary-little-endian `.ply` Gaussian-style splat rendering
- Debug normals view
- Wireframe pipeline variant
- Optional back-face culling
- Runtime ImGui debug overlay
- Simple camera/model inspection controls

### Vulkan Systems

- vk-bootstrap based instance/device/swapchain setup
- Vulkan Memory Allocator based GPU allocations
- Staging-buffer uploads for mesh and texture data
- Explicit image layout transitions
- `synchronization2` style barriers
- Two frames in flight
- Per-image render-finished semaphores
- Resize-safe swapchain recreation
- Validation-layer focused development workflow

### Tooling

- CMake 3.25+ build system
- vcpkg manifest dependencies
- Shader compilation through `glslc`
- Linux and Windows build presets
- Doxygen support
- RenderDoc-friendly frame structure
- Prototype evidence screenshots

---

## Project Scope

The renderer is intentionally scoped around a small vertical slice.

### Must Have

- [x] Window creation
- [x] Vulkan 1.3 setup
- [x] Swapchain creation and presentation
- [x] Dynamic Rendering
- [x] Mesh loading
- [x] Texture loading
- [x] Depth testing
- [x] Runtime debug UI
- [x] Resize-safe rendering
- [ ] Final report and evaluation

### Should Have

- [x] Wireframe view
- [x] Debug normals view
- [x] Back-face culling toggle
- [x] Frame timing display
- [x] In-app asset path display / browser workflow

### Stretch Goals

- [x] Mipmaps
- [ ] Anisotropic filtering
- [x] PBR material model
- [ ] Scene loading
- [ ] Shadow mapping
- [x] Experimental Gaussian-style ASCII/binary `.ply` splat rendering

Stretch goals are deliberately placed after the core renderer so they do not compete with the final report, testing, and evaluation.

---

## Key Design Decisions

| Decision | Reason |
| --- | --- |
| **Vulkan 1.3** | Exposes explicit GPU control over memory, synchronisation, command submission, and presentation. |
| **Dynamic Rendering** | Keeps the render path compact and avoids legacy render pass/framebuffer setup. |
| **vk-bootstrap** | Reduces setup boilerplate while leaving renderer logic explicit. |
| **Vulkan Memory Allocator** | Simplifies portable GPU memory allocation across different hardware. |
| **tinyobjloader + stb_image** | Supports the current one-model textured vertical slice without a large asset pipeline. |
| **ImGui overlay** | Makes frame metrics and debug render modes visible during live demos. |
| **Small scope** | Prioritises correctness, explanation, and evaluation over feature volume. |

---

## Software Requirements

| Type | Requirement |
| --- | --- |
| Operating system | Linux or Windows |
| Compiler | C++20 capable compiler |
| Build system | CMake 3.25+ |
| Graphics API | Vulkan 1.3 |
| Dependencies | vcpkg manifest mode |
| Debug tools | Vulkan validation layers and RenderDoc recommended |

---

## Minimum Hardware Requirements

| Type | Minimum |
| --- | --- |
| CPU | Modern quad-core CPU |
| RAM | 8 GB |
| GPU | Vulkan 1.3 capable GPU |
| Storage | Enough space for build outputs, dependencies, and assets |

The renderer is lightweight, but Vulkan driver support is required.

---

## Building for Windows and Linux

### First set up the repository

```bash
git clone https://github.com/Raiju-Deeq/FYP-Vulkan-Renderer.git
cd FYP-Vulkan-Renderer
```

### Linux

Install Vulkan tooling and development packages. On Arch-based systems:

```bash
sudo pacman -S vulkan-radeon vulkan-validation-layers cmake ninja git base-devel
```

Set up vcpkg:

```bash
git clone https://github.com/microsoft/vcpkg "$HOME/vcpkg"
"$HOME/vcpkg/bootstrap-vcpkg.sh"

export VCPKG_ROOT="$HOME/vcpkg"
```

Configure, build, and run:

```bash
cmake --preset linux-debug
cmake --build --preset linux-debug
VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation ./build/linux-debug/vulkan-renderer
```

### Windows

Set up vcpkg from PowerShell:

```powershell
git clone https://github.com/microsoft/vcpkg "$env:USERPROFILE\vcpkg"
& "$env:USERPROFILE\vcpkg\bootstrap-vcpkg.bat"

$env:VCPKG_ROOT = "$env:USERPROFILE\vcpkg"
$env:PATH = "$env:VCPKG_ROOT;$env:PATH"
```

Configure and build with the Windows presets:

```powershell
cmake --preset uni-debug
cmake --build --preset uni-debug
```

Run the executable from the generated build output folder.

---

## Build Presets

| Preset | Platform | Configuration | Notes |
| --- | --- | --- | --- |
| `linux-debug` | Linux | Debug | Validation focused development build |
| `linux-release` | Linux | Release | Optimised Linux build |
| `uni-debug` | Windows | Debug | University/Windows debug build |
| `uni-release` | Windows | Release | Optimised Windows build |

---

## Running the Renderer

After building, run the generated `vulkan-renderer` executable from the output directory. The runtime assets and compiled shaders are copied by CMake so the executable can locate the required model, texture, and SPIR-V files.

Useful things to check during a demo:

- the textured OBJ model appears correctly;
- the FPS/frame-time overlay is visible;
- wireframe mode changes the pipeline output;
- normals view changes the fragment shader output;
- resizing/minimising the window does not crash;
- validation layers do not report critical errors.

---

## Controls and Debug UI

The current prototype exposes renderer inspection through ImGui.

| Control / Display | Purpose |
| --- | --- |
| FPS and frame time | Shows runtime performance during the demo |
| Camera distance | Allows the model to be inspected more clearly |
| Wireframe toggle | Shows mesh topology and pipeline variant switching |
| Back-face culling toggle | Demonstrates rasterizer state changes |
| Normals view | Visualises imported/interpolated normals |
| Asset path display/browser | Shows which model and texture are active |

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
├── index.html
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
│   ├── gaussian_splat.hpp / gaussian_splat.cpp
│   └── texture.hpp / texture.cpp
├── shaders/
│   ├── triangle.vert
│   ├── triangle.frag
│   ├── mesh.vert
│   ├── mesh.frag
│   ├── splat.vert
│   └── splat.frag
├── assets/
│   ├── models/
│   └── textures/
└── docs/
    ├── dev-log/
    ├── prototype-evidence/
    └── wiki/
```

### Core Systems

| System | Responsibility |
| --- | --- |
| `Application` | Startup, main loop, UI, asset selection, resize handling, teardown |
| `AppWindow` | GLFW window creation, event polling, framebuffer state |
| `VulkanContext` | Instance, device, queues, surface, debug messenger, VMA allocator |
| `SwapChain` | Swapchain images, colour views, depth resources, rebuild logic |
| `GraphicsPipeline` | Shader modules, descriptor layout, pipeline layout, rasterizer variants |
| `Renderer` | Command buffers, fences, semaphores, frame submission, ImGui rendering |
| `GpuBuffer` | Buffer/image creation, staging uploads, layout transitions |
| `Mesh` | OBJ loading, vertex/index data, GPU mesh buffers |
| `GaussianSplat` | ASCII/binary `.ply` point ingestion, storage buffer upload, soft billboard splat rendering |
| `Material` | Texture loading, image view, sampler, descriptor set |

---

## How a Frame Is Rendered

At a high level, one frame follows this path:

1. Poll window events.
2. Update ImGui and frame metrics.
3. Build the model-view-projection matrix.
4. Wait for the current frame fence.
5. Acquire the next swapchain image.
6. Reset and record the command buffer.
7. Transition colour/depth images into attachment layouts.
8. Begin Dynamic Rendering.
9. Bind the selected pipeline variant.
10. Bind the texture descriptor set.
11. Bind vertex and index buffers.
12. Push MVP/debug constants.
13. Draw indexed geometry.
14. Render the ImGui overlay.
15. End Dynamic Rendering.
16. Transition the colour image for presentation.
17. Submit the command buffer.
18. Present the swapchain image.
19. Rebuild swapchain-dependent resources if required.

---

## Dependencies

All third-party dependencies are managed through vcpkg manifest mode.

| Library | Purpose |
| --- | --- |
| [Vulkan SDK](https://vulkan.lunarg.com/) | Core graphics API and tooling |
| [vk-bootstrap](https://github.com/charles-lunarg/vk-bootstrap) | Vulkan instance/device/swapchain setup |
| [GLFW](https://www.glfw.org/) | Window creation and input |
| [GLM](https://github.com/g-truc/glm) | Math |
| [Vulkan Memory Allocator](https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/) | GPU memory allocation |
| [tinyobjloader](https://github.com/tinyobjloader/tinyobjloader) | OBJ model loading |
| [stb_image](https://github.com/nothings/stb) | Texture loading |
| [Dear ImGui](https://github.com/ocornut/imgui) | Debug UI |
| [spdlog](https://github.com/gabime/spdlog) | Logging |

---

## First Places to Look

If you are reading the code for the first time, start here:

1. `src/main.cpp`  
   Entry point. Creates and runs the application.

2. `src/application.cpp`  
   High-level lifecycle, debug UI, asset selection, and render-loop coordination.

3. `src/frame_data.cpp`
   Frame synchronisation and command submission logic.

4. `src/graphics_pipeline.cpp`  
   Shader modules, pipeline layout, Dynamic Rendering pipeline state, and rasterizer variants.

5. `src/mesh.cpp` and `src/texture.cpp`  
   CPU asset loading and GPU upload paths.

6. `src/swapchain.cpp`  
   Swapchain creation, depth resources, and resize recovery.

---

## Coding Standards

- Use C++20.
- Prefer RAII-style ownership and explicit teardown.
- Keep Vulkan object ownership clear.
- Use Dynamic Rendering only for the main render path.
- Avoid `VkRenderPass` and `VkFramebuffer`.
- Use validation layers during development.
- Treat validation errors as blocking defects.
- Keep synchronisation explicit and explainable.
- Keep scope controlled: core renderer first, stretch features later.
- Document major systems with Doxygen comments.

---

## Report and Evaluation Focus

The final report focuses on:

- why Vulkan 1.3 was chosen;
- why Dynamic Rendering was used;
- how vk-bootstrap reduced setup complexity;
- how swapchain recreation was handled safely;
- how mesh and texture uploads work;
- how synchronisation avoids unsafe resource reuse;
- how debug views support testing and explanation;
- what was tested using screenshots, logs, validation layers, and RenderDoc;
- what remains outside the scope of the project.

---

## Roadmap

### Current Milestone

- Textured OBJ vertical slice
- Dynamic Rendering
- Depth path
- ImGui debug controls
- Resize-safe swapchain handling

### Next

- More screenshots and RenderDoc evidence
- Cleaner documentation site
- Build troubleshooting guide
- Better camera controls
- Improved asset loading notes

### Future

- Mipmaps
- PBR material path
- Shadow mapping
- Scene loading
- Gaussian splat rendering experiment

---

## Credits for 3D Assets Used

The prototype currently uses small demonstration assets stored under `assets/models/` and `assets/textures/`.

If larger third-party assets are added later, this section should list:

- asset name;
- original author;
- licence;
- download/source link;
- any modifications made for the renderer.

---

## Project Context

**Programme:** BSc (Hons) Games Production, De Montfort University, Leicester  
**Author:** Mohamed Deeq Mohamed  
**Supervisor:** Salim Hasshu  
**Focus:** Vulkan rendering fundamentals, explicit GPU programming, technical reflection, and future neural rendering research

---

## References

- [Vulkan 1.3 Specification](https://registry.khronos.org/vulkan/specs/1.3/html/)
- [Vulkan Guide](https://vkguide.dev/)
- [Vulkan Dynamic Rendering](https://docs.vulkan.org/refpages/latest/refpages/source/VK_KHR_dynamic_rendering.html)
- [Swapchain Semaphore Reuse](https://docs.vulkan.org/guide/latest/swapchain_semaphore_reuse.html)
- [vk-bootstrap](https://github.com/charles-lunarg/vk-bootstrap)
- [Vulkan Memory Allocator](https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/)
- [RenderDoc](https://renderdoc.org/)
- [tinyobjloader](https://github.com/tinyobjloader/tinyobjloader)
- [stb](https://github.com/nothings/stb)
- [3D Gaussian Splatting, Kerbl et al. 2023](https://repo-sam.inria.fr/fungraph/3d-gaussian-splatting/)

---

## License

This project is licensed under the [MIT License](LICENSE).
