# Changelog

All notable project changes are tracked here.

## v0.4.0 - 2026-05-26

Gaussian splatting implementation milestone focused on moving the stretch renderer
from a point-cloud style preview into a GPU-projected Gaussian pipeline.

### Added

- GPU-projected Gaussian splat renderer backed by a device-local storage buffer.
- ASCII and binary-little-endian `.ply` loading for Gaussian splat assets.
- Support for common 3DGS PLY attributes: position, RGB colour, degree-0 spherical harmonic colour, opacity logits, scale, and quaternion rotation.
- Import-time point cloud normalisation so loaded splats appear in the existing camera view without manual transform edits.
- Import-time 3D covariance precomputation from activated Gaussian scale and rotation.
- Dedicated `splat.vert` projection path that expands each Gaussian into a screen-space quad on the GPU.
- Dedicated `splat.frag` conic evaluation with pre-multiplied alpha output.
- Alpha-blended splat graphics pipeline with depth testing enabled and depth writes disabled.
- Runtime ImGui controls for showing splats, adjusting splat radius, adjusting splat opacity, and hot-swapping `.ply` splat files.
- Independent OBJ mesh visibility toggle so mesh rendering and splat rendering can be compared or shown separately.
- Default `assets/models/point_cloud.ply` loading when available.
- Swapchain resize handling that rebuilds both mesh and splat pipelines while preserving uploaded splat data.
- Release shader fallback instructions for compiling the new splat shaders manually when `glslc` is not found.

### Changed

- Gaussian splat rendering no longer projects, sorts, and uploads splat draw data on the CPU every frame.
- The render loop now passes camera projection data and splat controls through push constants for the splat pipeline.
- Renderer frame recording can draw the OBJ mesh, Gaussian splats, and ImGui overlay in the same Dynamic Rendering scope.
- Swapchain initialisation now cleans up partially created resources if image or image-view retrieval fails.

### Known Limitations

- Gaussian splatting is still a stretch-objective preview rather than a full research-grade 3DGS renderer.
- Full GPU depth sorting is not implemented yet, so dense transparent regions may not composite perfectly.
- Higher-order spherical harmonics are not evaluated; colour uses RGB values or degree-0 SH colour reconstruction.
- Runtime still requires a Vulkan 1.3 capable GPU/driver.
- Windows and Linux binary packages are provided for releases.

## v0.3.0 - 2026-05-24

Gaussian splat prototype milestone release focused on the C3 stretch objective.

### Added

- Experimental Gaussian-style `.ply` splat rendering path.
- ASCII and binary-little-endian PLY ingestion for point cloud assets.
- Postshot-style `point_cloud.ply` support using position, DC colour, opacity, and scale data.
- VMA-backed GPU storage buffer upload for splat point records.
- Separate alpha-blended splat graphics pipeline using `splat.vert` and `splat.frag`.
- Runtime ImGui controls for showing/hiding OBJ meshes and splats independently.
- Runtime `.ply` hot-swapping through the existing ImGui asset browser workflow.
- Splat radius and opacity preview controls.
- Updated README screenshots and assessment documents for the Gaussian splat milestone.

### Known Limitations

- Gaussian splatting is currently a preview-style stretch implementation, not full research-grade 3DGS.
- The splat renderer does not yet implement covariance projection, quaternion rotation, full spherical harmonics, depth sorting, or high-quality alpha compositing.
- Runtime still requires a Vulkan 1.3 capable GPU/driver.
- Windows and Linux binary packages are provided for releases.

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
