# Gaussian Splat Stretch Goal

The C3 stretch path is now implemented as a GPU-projected Gaussian splat
preview. I load ASCII or binary-little-endian `.ply` files, activate the common
3DGS attributes, recover colour from RGB or degree-0 spherical harmonic values,
precompute each Gaussian covariance once, and upload the compact records into a
device-local storage buffer.

The important refactor was moving the per-frame work off the CPU. My first
working version projected, sorted and uploaded a draw buffer every frame, which
became too slow for larger point clouds. The current version keeps the full
loaded point count and lets `splat.vert` project every Gaussian on the GPU,
expand it into a screen-space quad, and pass the conic/colour/opacity values to
the fragment shader for pre-multiplied alpha blending.

This is still a stretch-objective preview rather than a complete research-grade
3DGS renderer. The main known limitation is that I do not yet perform full GPU
depth sorting for alpha compositing, so very dense transparent regions may not
composite perfectly. The important improvement is that the renderer now uses
the GPU for the expensive camera-dependent projection path instead of reducing
the number of Gaussians just to stay interactive.
