# Release Guide

This project uses semantic version tags for GitHub Releases.

## v0.3.0 Release Checklist

- Confirm the working tree is clean with `git status`.
- Confirm `CMakeLists.txt` and `vcpkg.json` both report `0.3.0`.
- Run a release build locally:

```powershell
cmake --preset uni-release
cmake --build --preset uni-release
```

- Push the release prep commit to `main`.
- Create and push the release tag:

```powershell
git tag -a v0.3.0 -m "Raiju Renderer v0.3.0"
git push origin main
git push origin v0.3.0
```

The `release.yml` workflow will create a GitHub Release from the tag and attach:

- `FYP-Vulkan-Renderer-v0.3.0-source.zip`
- `Raiju-Renderer-v0.3.0-windows-x64.zip`
- `Raiju-Renderer-v0.3.0-linux-x64.tar.gz`

The Windows zip contains the executable, runtime DLLs, assets, shaders, README, and license.
The Linux tarball contains the executable, assets, shaders, README, and license.

## Updating an Existing Release

If the release already exists, push this workflow update to `main`, then run **GitHub Release** manually from the Actions tab using the existing tag, for example `v0.3.0`.

The workflow will rebuild the Windows and Linux packages and upload them to the existing release with `--clobber`.

## Release Notes Template

```markdown
## Raiju Renderer v0.3.0

Gaussian splat prototype milestone release of the Vulkan renderer.

### Highlights

- Vulkan 1.3 Dynamic Rendering path.
- Textured OBJ rendering with depth testing.
- Cook-Torrance direct PBR lighting.
- Mipmap generation for loaded textures.
- Optional PBR texture slots for base colour, metallic/roughness, AO, emissive, and reserved normal maps.
- Runtime ImGui debug overlay.
- Wireframe, back-face culling, normals view, and frame timing controls.
- Experimental Gaussian-style PLY splat rendering.
- Runtime OBJ/splat visibility toggles.
- Linux and Windows CMake/vcpkg build workflow.

### Requirements

- Vulkan 1.3 capable GPU and driver.
- CMake 3.25+.
- C++20 capable compiler.
- vcpkg manifest mode dependencies.

### Notes

This is a prototype release for a Final Year Project. Windows and Linux binary packages are provided, and the project can still be built from source using the instructions in `README.md`.
```
