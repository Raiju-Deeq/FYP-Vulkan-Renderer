#version 450
// M1 - Baseline triangle vertex shader
// Hardcoded clip-space positions for the initial triangle render.
// No vertex buffers yet - positions are baked in as a constant array
// indexed by gl_VertexIndex. Replaced in M2 with a proper vertex buffer.

layout(location = 0) out vec3 fragColor;

void main() {
    // Hardcoded NDC positions for a centred triangle
    const vec2 positions[3] = vec2[](
        vec2( 0.0, -0.5),
        vec2( 0.5,  0.5),
        vec2(-0.5,  0.5)
    );

    // Per-vertex colours
    const vec3 colors[3] = vec3[](
        vec3(1.0, 0.0, 0.0),  // red
        vec3(0.0, 1.0, 0.0),  // green
        vec3(0.0, 0.0, 1.0)   // blue
    );

    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    fragColor   = colors[gl_VertexIndex];
}
