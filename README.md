<div align="center">

<br />

# Vulkan Renderer — C++20

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

</div>

---

## Overview

A real-time 3D renderer built against the **Vulkan 1.3 API** in **C++20**, submitted as the Final Year Project for BSc (Hons) Games Production at De Montfort University. Scoped deliberately as a **renderer, not an engine** — no physics, ECS, audio, or scene graph.

Built entirely around **Vulkan 1.3 Dynamic Rendering** — no `VkRenderPass`, no `VkFramebuffer`, ever. All GPU memory goes through **VMA**. All image layout transitions are explicit via `synchronization2`. Vulkan initialisation is handled by **vk-bootstrap**.

This project also serves as a pilot for **PhD research in neural rendering at DMU (October 2026)**, with the M6 Gaussian Splatting milestone directly bridging into that research direction.

📋 **Full project documentation, milestones, architecture, dev logs, and research notes live in the [Project Vault →](docs/FYP-Vault/Home.md)**

---

## Getting Started

Requires a **Vulkan 1.3-capable GPU**, a C++20 toolchain, and CMake 3.25+.

```bash
git clone https://github.com/Raiju-Deeq/FYP-Vulkan-Renderer.git
cd FYP-Vulkan-Renderer
```

### Linux (Arch)

```bash
sudo pacman -S vulkan-radeon vulkan-validation-layers cmake ninja git \
               base-devel autoconf autoconf-archive automake libtool

git clone https://github.com/microsoft/vcpkg "$HOME/vcpkg"
"$HOME/vcpkg/bootstrap-vcpkg.sh"
export VCPKG_ROOT="$HOME/vcpkg"

cmake --preset linux-debug
cmake --build --preset linux-debug
VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation ./build/linux-debug/vulkan-renderer
```

### Windows (DMU lab — no admin required)

```bat
git clone https://github.com/microsoft/vcpkg %USERPROFILE%\vcpkg
%USERPROFILE%\vcpkg\bootstrap-vcpkg.bat
set VCPKG_ROOT=%USERPROFILE%\vcpkg

cmake --preset uni-debug
cmake --build --preset uni-debug
build\uni-debug\vulkan-renderer.exe
```

| Preset | Platform | Config | ASan + UBSan |
|--------|----------|--------|--------------|
| `linux-debug` | Arch Linux | Debug | ✅ |
| `linux-release` | Arch Linux | Release | ❌ |
| `uni-debug` | Windows (DMU) | Debug | ✅ |
| `uni-release` | Windows (DMU) | Release | ❌ |

---

## API Docs (Doxygen)

All public APIs carry Doxygen headers. HTML reference is built on demand — **not committed**.

```bash
# Linux
cmake --build --preset linux-debug --target docs
xdg-open docs/doxygen/html/index.html

# Windows
cmake --build --preset uni-debug --target docs
start docs\doxygen\html\index.html
```

Prerequisites: `sudo pacman -S doxygen graphviz` / [doxygen.nl](https://www.doxygen.nl) + [graphviz.org](https://graphviz.org) on Windows.

---

## Project Layout

```
FYP-Vulkan-Renderer/
├── src/                    # C++20 source — VulkanContext, SwapChain, Pipeline,
│                           #   Renderer, Mesh, Material, GaussianSplat
├── shaders/                # GLSL source (triangle, mesh, pbr, splat)
│                           #   *.spv compiled by CMake — gitignored
├── assets/
│   ├── models/             # OBJ meshes + textures (M3)
│   └── splats/             # .ply Gaussian splat files (M6)
├── docs/
│   ├── Doxyfile
│   ├── doxygen/            # Generated HTML — gitignored, build locally
│   └── FYP-Vault/          # Obsidian knowledge vault
│       ├── Home.md         # ← start here
│       ├── 01-Plans/
│       ├── 02-Dev-Log/
│       ├── 03-Learnings/
│       └── Research/
├── .claude/skills/         # Obsidian Skills for Claude Code
├── CLAUDE.md               # Claude Code project specification
├── CMakeLists.txt
└── vcpkg.json
```

---

## Knowledge System

Project notes, dev logs, implementation plans, and research are maintained in an [Obsidian](https://obsidian.md/) vault at [`docs/FYP-Vault/`](docs/FYP-Vault/), version-controlled alongside the source. The vault is authored with [Claude Code](https://claude.ai/code) using the [obsidian-skills](https://github.com/kepano/obsidian-skills) plugin and feeds directly into the technical report.

Open in Obsidian: **File → Open vault → `docs/FYP-Vault/`**

---

## Academic Context

Submitted for BSc (Hons) Games Production at **De Montfort University, Leicester**, supervised by **Salim Hashu** and **Dr Conor Fahy**. Conceived as a foundation for PhD research in neural rendering at DMU commencing October 2026.

**Contact:** [mdeeq0@me.com](mailto:mdeeq0@me.com)

---

<div align="center">

<br />

*No `VkRenderPass`. No `VkFramebuffer`. Ever.*

<br />

</div>
