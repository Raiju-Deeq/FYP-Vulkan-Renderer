# Research - PBR Shading (Cook-Torrance BRDF)

**Date:** 2026-03-30
**Milestone:** M4 (Could Have)
**Status:** Research / Pre-implementation

---

## Overview

Physically Based Rendering (PBR) uses the Cook-Torrance BRDF to model how light interacts with surfaces in a physically plausible way. It replaced ad-hoc Phong shading in game engines around 2012-2014 (Unreal Engine 4, Unity 5).

---

## Cook-Torrance BRDF

The reflectance equation:

$$f_r = k_d \frac{c}{\pi} + k_s \frac{DFG}{4(\omega_o \cdot n)(\omega_i \cdot n)}$$

Components:
- **D** - Normal Distribution Function (GGX/Trowbridge-Reitz): models microfacet distribution
- **F** - Fresnel equation (Schlick approximation): reflection ratio at grazing angles
- **G** - Geometry function (Smith): models self-shadowing of microfacets

---

## Material Parameters

| Parameter | Range | Description |
|-----------|-------|-------------|
| `albedo` | [0,1]³ | Base colour |
| `metallic` | [0,1] | 0 = dielectric, 1 = metal |
| `roughness` | [0,1] | 0 = mirror, 1 = fully rough |
| `ao` | [0,1] | Ambient occlusion |

---

## Renderer Integration

- Lives in `src/Material.h / .cpp`
- `MaterialUBO` passed as std140-aligned uniform buffer
- Fragment shader: `shaders/pbr.frag`
- Requires texture maps: albedo, normal, metallic-roughness, AO

---

## Resources

- **learnopengl.com/PBR** - best practical introduction
- **Epic Games PBR notes (2013)** - foundational industry reference
- **vkguide.dev** - Vulkan-specific descriptor set setup for textures

---

*FYP - Vulkan Renderer in C++20 · Mohamed Deeq Mohamed · P2884884 · De Montfort University*
