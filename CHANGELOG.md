# Changelog

All notable project changes are tracked here.

## v0.1.0 - 2026-05-12

Initial public prototype release of Raiju Renderer.

### Added

- Vulkan 1.3 renderer using Dynamic Rendering.
- GLFW window creation and resize-safe swapchain management.
- Textured OBJ loading with `tinyobjloader` and `stb_image`.
- GPU mesh and texture uploads through Vulkan Memory Allocator.
- Depth testing, basic diffuse lighting, debug normals view, and wireframe pipeline variant.
- Runtime ImGui overlay with frame timing, render toggles, and simple asset browsing.
- CMake and vcpkg manifest build flow for Linux and Windows.
- GitHub Actions Linux build validation.
- Project documentation, screenshots, and GitHub Pages product page.

### Known Limitations

- Prototype scope: this is not a full engine or packaged end-user application.
- Runtime requires a Vulkan 1.3 capable GPU/driver and local build dependencies.
- Windows binary packages are provided for releases; Linux users should build from source.
- Mipmaps, anisotropic filtering, PBR materials, scene loading, shadows, and Gaussian splatting remain future work.
