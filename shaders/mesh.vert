#version 450

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    // The fragment shader consumes these material/light groups.  The vertex
    // shader only needs mvp, but keeping one identical block across both
    // stages makes the CPU/GPU push-constant layout easier to reason about.
    vec4 baseColorFactor;         // rgb = base colour factor
    vec4 debugOptions;            // x = debug mode
    vec4 lightDirectionIntensity; // xyz = light direction, w = intensity
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec3 fragNormal;

void main() {
    gl_Position = pc.mvp * vec4(inPosition, 1.0);
    fragTexCoord = inTexCoord;
    fragNormal = normalize(inNormal);
}
