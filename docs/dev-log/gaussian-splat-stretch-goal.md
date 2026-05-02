# Gaussian Splat Stretch Goal

The previous `GaussianSplat` C3 scaffold was not part of the requested runtime
source layout, so it has been moved out of the build-facing `src/` structure.

The design intent remains recorded here for the report: a future 3D Gaussian
Splatting path would load binary-little-endian `.ply` scenes, repack positions,
opacity, scale, rotation and degree-0 spherical harmonic colour into CPU
`GaussianPoint` records, upload them into a device-local SSBO, depth-sort them
back-to-front each frame, and draw each Gaussian as a screen-aligned quad for
alpha compositing.
