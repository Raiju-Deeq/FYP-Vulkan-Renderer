#version 450
// =============================================================================
// mesh.vert — Geometry + UV vertex shader  [M2/M3+]
// =============================================================================
// Replaces triangle.vert from M2 onwards. Reads per-vertex data from a real
// vertex buffer (position, normal, UV) bound via vkCmdBindVertexBuffers, and
// transforms it using the MVP matrices delivered through a UBO descriptor set.
//
// STATUS:
//   - GLSL side (this file): complete. The UBO layout and transform math are
//     written correctly and ready to go.
//   - C++ side (NOT YET DONE in M1): you need to create a VkDescriptorSetLayout,
//     allocate one VkDescriptorSet per in-flight frame, write the actual
//     VkBuffer into each set, and call vkCmdBindDescriptorSets before drawing.
//     Until descriptor sets are bound in M2, the UBO reads will produce
//     garbage and the shader will not function correctly.
//
// UBO layout expected by the C++ side:
//   set = 0, binding = 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, vertex stage only
//
// Vertex attribute layout (must match Pipeline.cpp binding/attribute descriptions
// and the Vertex struct in Mesh.h):
//   location 0  vec3  position   (3 × float32, offset 0)
//   location 1  vec3  normal     (3 × float32, offset 12)
//   location 2  vec2  texCoord   (2 × float32, offset 24)
// =============================================================================

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragWorldPos;

// MVP uniform buffer — set 0, binding 0
// One copy per in-flight frame to avoid CPU/GPU racing on the same buffer.
// The C++ struct UniformBufferObject in Renderer.h mirrors this layout exactly.
layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;  // object → world space transform
    mat4 view;   // world → camera space (glm::lookAt)
    mat4 proj;   // camera → clip space  (glm::perspective, Y-flipped for Vulkan)
} ubo;

void main() {
    // Transform position to world space first so we can pass fragWorldPos
    // to the fragment shader for lighting calculations (M3+).
    vec4 worldPos = ubo.model * vec4(inPosition, 1.0);
    fragWorldPos  = worldPos.xyz;

    // Transform normal by the inverse-transpose of the model matrix so that
    // non-uniform scaling doesn't distort the normals. This is a standard
    // PBR pre-requisite — skipping it causes lighting to look wrong on
    // scaled geometry.
    fragNormal = mat3(transpose(inverse(ubo.model))) * inNormal;

    // Pass UVs through unchanged — the fragment shader samples the albedo
    // texture at these coordinates (M3).
    fragTexCoord = inTexCoord;

    // Final clip-space position: proj * view * world.
    // Note: ubo.proj already has the Y-axis flipped (proj[1][1] *= -1 in
    // Renderer.cpp) to account for Vulkan's inverted NDC Y axis vs OpenGL.
    gl_Position = ubo.proj * ubo.view * worldPos;
}
