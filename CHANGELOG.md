# Changelog

All notable project changes are tracked here.

## v0.2.0 - 2026-05-19

Current renderer milestone release focused on PBR material rendering and texture quality improvements.

### Added

- Cook-Torrance direct PBR lighting path for textured OBJ rendering.
- Mipmap generation for loaded textures.
- Optional PBR texture slots for base colour, metallic/roughness, ambient occlusion, emissive, and reserved normal maps.
- Updated shader inputs and renderer-side texture binding flow for the expanded material data.
- Updated milestone screenshot and documentation to reflect the current renderer state.

### Known Limitations

- Prototype scope: this is still a focused renderer milestone rather than a full engine or packaged end-user application.
- Runtime requires a Vulkan 1.3 capable GPU/driver and local build dependencies.
- Windows and Linux binary packages are provided for releases.
- Anisotropic filtering, full scene loading, shadow mapping, and Gaussian splatting remain future work.

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
- GitHub Release packaging for Windows and Linux binary builds.
- Project documentation, screenshots, and GitHub Pages product page.

### Known Limitations

- Prototype scope: this is not a full engine or packaged end-user application.
- Runtime requires a Vulkan 1.3 capable GPU/driver and local build dependencies.
- Windows and Linux binary packages are provided for releases.
- Mipmaps, anisotropic filtering, PBR materials, scene loading, shadows, and Gaussian splatting remain future work.
