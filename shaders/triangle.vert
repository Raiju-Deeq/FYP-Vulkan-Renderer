#version 450
// =============================================================================
// triangle.vert — Baseline triangle vertex shader  [M1 only]
// =============================================================================
// Proof-of-concept shader used to verify the full Vulkan pipeline
// (instance → device → swapchain → pipeline → renderer) without involving
// vertex buffers, descriptor sets, or UBOs.
//
// Positions are hardcoded as a const array indexed by gl_VertexIndex.
// main.cpp calls vkCmdDraw(cmd, 3, 1, 0, 0) — no vkCmdBindVertexBuffers.
//
// Replaced in M2 by mesh.vert + a real VkBuffer vertex buffer.
// =============================================================================

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
