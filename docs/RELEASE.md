# Release Guide

This project uses semantic version tags for GitHub Releases.

## v0.1.0 Release Checklist

- Confirm the working tree is clean with `git status`.
- Confirm `CMakeLists.txt` and `vcpkg.json` both report `0.1.0`.
- Run a release build locally:

```powershell
cmake --preset uni-release
cmake --build --preset uni-release
```

- Push the release prep commit to `main`.
- Create and push the release tag:

```powershell
git tag -a v0.1.0 -m "Raiju Renderer v0.1.0"
git push origin main
git push origin v0.1.0
```

The `release.yml` workflow will create a GitHub Release from the tag and attach:

- `FYP-Vulkan-Renderer-v0.1.0-source.zip`
- `Raiju-Renderer-v0.1.0-windows-x64.zip`

The Windows zip contains the executable, runtime DLLs, assets, shaders, README, and license.

## Updating an Existing Release

If the release already exists, push this workflow update to `main`, then run **GitHub Release** manually from the Actions tab using the existing tag, for example `v0.1.0`.

The workflow will rebuild the Windows package and upload it to the existing release with `--clobber`.

## Release Notes Template

```markdown
## Raiju Renderer v0.1.0

Initial public prototype release of the Vulkan renderer.

### Highlights

- Vulkan 1.3 Dynamic Rendering path.
- Textured OBJ rendering with depth testing.
- Runtime ImGui debug overlay.
- Wireframe, back-face culling, normals view, and frame timing controls.
- Linux and Windows CMake/vcpkg build workflow.

### Requirements

- Vulkan 1.3 capable GPU and driver.
- CMake 3.25+.
- C++20 capable compiler.
- vcpkg manifest mode dependencies.

### Notes

This is a source release for a Final Year Project prototype. Build from source using the instructions in `README.md`.
```
