#version 450
// M4 - Cook-Torrance PBR fragment shader  [COULD HAVE]
// Implements a physically-based BRDF:
//   - Lambertian diffuse term
//   - GGX normal distribution function (NDF)
//   - Smith geometry masking-shadowing term
//   - Schlick Fresnel approximation
//
// Material parameters (albedo, metallic, roughness, ao) are supplied
// via a push constant block and controllable at runtime via Dear ImGui.
//
// STATUS: Stub - full implementation in M4 (Week 7)

#define PI 3.14159265358979323846

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragWorldPos;

layout(location = 0) out vec4 outColor;

// Combined image sampler - bound in M3+
layout(set = 0, binding = 1) uniform sampler2D albedoMap;

// PBR material parameters via push constants
// Adjustable at runtime via Dear ImGui panel (M4)
layout(push_constant) uniform PBRParams {
    vec3  albedo;
    float metallic;
    float roughness;
    float ao;
    vec3  lightPos;
    vec3  lightColor;
    vec3  camPos;
} pbr;

// ── GGX Normal Distribution Function ────────────────────────────────────
float distributionGGX(vec3 N, vec3 H, float roughness) {
    // TODO (M4): implement
    return 0.0;
}

// ── Smith Geometry Masking-Shadowing ────────────────────────────────────
float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    // TODO (M4): implement
    return 0.0;
}

// ── Schlick Fresnel Approximation ───────────────────────────────────────
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    // TODO (M4): implement
    return F0;
}

void main() {
    // Stub output - passes through albedo colour until M4 is implemented.
    // Replace this entire main() in M4 with the full Cook-Torrance loop.
    outColor = vec4(pbr.albedo, 1.0);
}
