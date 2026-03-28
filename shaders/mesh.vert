#version 450
// M2/M3 — Geometry + UV vertex shader
// Used from M2 (cube) onwards. Receives per-vertex position, normal,
// and UV coordinates from vertex buffers and transforms them by the
// MVP matrix delivered via a Uniform Buffer Object (UBO).
//
// TODO (M2): bind UBO set 0 binding 0 and apply MVP transform
// TODO (M3): pass UV to fragment shader for texture sampling

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragWorldPos;

// UBO — set 0, binding 0
// Will be populated in M2 once descriptor sets are implemented.
layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

void main() {
    vec4 worldPos   = ubo.model * vec4(inPosition, 1.0);
    fragWorldPos    = worldPos.xyz;
    fragNormal      = mat3(transpose(inverse(ubo.model))) * inNormal;
    fragTexCoord    = inTexCoord;
    gl_Position     = ubo.proj * ubo.view * worldPos;
}
