#version 450
// =============================================================================
// triangle.frag — Baseline triangle fragment shader  [M1 only]
// =============================================================================
// Paired with triangle.vert. Receives the interpolated colour (GPU-blended
// across the three red/green/blue vertices) and writes it straight to the
// colour attachment — no lighting calculation, no texture sampling.
//
// Replaced in M2 by pbr.frag which will eventually do proper shading.
// =============================================================================

layout(location = 0) in  vec3 fragColor;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(fragColor, 1.0);
}
