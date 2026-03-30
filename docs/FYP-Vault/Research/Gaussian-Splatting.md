# Research — 3D Gaussian Splatting

**Date:** 2026-03-30
**Milestone:** M6 (stretch)
**Status:** Research / Pre-implementation

---

## Overview

3D Gaussian Splatting (3DGS) is a real-time novel view synthesis technique introduced by Kerbl et al. (2023). Rather than a neural network, it represents a scene as millions of 3D Gaussian primitives, each with position, opacity, colour (via spherical harmonics), scale, and rotation.

---

## Relevance to This Project

- **M6 stretch goal** — render a pre-trained `.ply` splat file in the Vulkan renderer
- **PhD research direction** — neural rendering at DMU builds directly on this
- The renderer's Dynamic Rendering architecture makes adding a splat pass additive: a second `vkCmdBeginRendering` with `VK_ATTACHMENT_LOAD_OP_LOAD` composites splats onto the main scene

---

## Key Concepts

### Gaussian Primitives
Each splat stores:
- `position` (xyz)
- `opacity` (α)
- `scale` (3D)
- `rotation` (quaternion)
- Spherical harmonic coefficients (colour)

### Rendering Pipeline
1. **Upload** `.ply` splat data to GPU storage buffer via VMA
2. **Depth sort** splats CPU-side per frame (radix sort or `std::sort`)
3. **Project** 3D Gaussians to screen-space ellipses in vertex/geometry shader
4. **Alpha composite** splats front-to-back using additive blending

### M6 Scope Limit
- Render pre-trained `.ply` files only — no training pipeline
- Cap at 1M splats for performance
- Hard gate: M6 cannot begin until M3 is signed off

---

## Key Papers

- Kerbl et al. (2023) — *3D Gaussian Splatting for Real-Time Radiance Field Rendering* — SIGGRAPH 2023
- Zwicker et al. (2002) — *EWA Splatting* — foundational splatting theory

---

## Implementation Notes

```cpp
// GaussianSplat module (src/GaussianSplat.h)
struct GaussianPoint {
    glm::vec3 position;
    float opacity;
    glm::vec3 scale;
    glm::quat rotation;
    // SH coefficients...
};
```

---

*FYP — Vulkan Renderer in C++20 · Mohamed Deeq Mohamed · P2884884 · De Montfort University*
