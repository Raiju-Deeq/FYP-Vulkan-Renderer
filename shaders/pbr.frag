#version 450
// =============================================================================
// pbr.frag — Cook-Torrance PBR fragment shader  [M4 — COULD HAVE]
// =============================================================================
// Implements a physically-based BRDF composed of three terms:
//
//   L_out = (k_d * albedo/PI  +  k_s * specular) * L_in * dot(N, L)
//
//   where:
//     k_d      = (1 - F) * (1 - metallic)   — diffuse weight (metals skip diffuse)
//     k_s      = F                           — specular weight driven by Fresnel
//     specular = (D * G * F) / (4 * dot(N,V) * dot(N,L))
//       D = GGX normal distribution   — controls how wide/sharp the specular lobe is
//       G = Smith geometry term       — accounts for micro-surface self-shadowing
//       F = Schlick Fresnel           — reflection intensity varies with view angle
//
// Material inputs come from push constants so they're editable live via ImGui (M4).
// Albedo texture sampled from set=0, binding=1 (bound in M3 via Material class).
//
// STATUS: Stub — the three BRDF helper functions return placeholder values.
//         main() outputs flat push-constant albedo until M4 is implemented.
// =============================================================================

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

// ── GGX Normal Distribution Function (D term) ───────────────────────────
// Computes the statistical distribution of microfacet normals aligned with H.
// High roughness = wide lobe (many normals misaligned), low roughness = sharp
// specular highlight. The formula is:
//   D = alpha^2 / (PI * (dot(N,H)^2 * (alpha^2 - 1) + 1)^2)
// where alpha = roughness^2 (perceptually linear roughness remapping).
// Returns 0.0 until M4 — placeholder to keep the shader compilable.
float distributionGGX(vec3 N, vec3 H, float roughness) {
    // TODO (M4): implement Cook-Torrance GGX NDF
    return 0.0;
}

// ── Smith Geometry Masking-Shadowing (G term) ────────────────────────────
// Models micro-surface self-shadowing: rough surfaces block more of their own
// reflected light. Smith combines two SchlickGGX terms — one for the view
// direction (masking) and one for the light direction (shadowing):
//   G = G_schlick(N,V,k) * G_schlick(N,L,k)
// where k = (roughness+1)^2 / 8 for direct lighting.
// Returns 0.0 until M4 — placeholder to keep the shader compilable.
float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    // TODO (M4): implement Smith geometry term (SchlickGGX × SchlickGGX)
    return 0.0;
}

// ── Schlick Fresnel Approximation (F term) ──────────────────────────────
// Fresnel describes how much light is reflected vs refracted at a surface
// as a function of viewing angle. At grazing angles, even rough dielectrics
// look specular (this is why wet road edges look mirror-like).
// Schlick's approximation:
//   F = F0 + (1 - F0) * (1 - cosTheta)^5
// F0 is the surface's base reflectance at normal incidence:
//   dielectrics: ~vec3(0.04)
//   metals: use their albedo colour as F0.
// Returns F0 until M4 — placeholder to keep the shader compilable.
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    // TODO (M4): implement F0 + (1 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0)
    return F0;
}

void main() {
    // Stub output - passes through albedo colour until M4 is implemented.
    // Replace this entire main() in M4 with the full Cook-Torrance loop.
    outColor = vec4(pbr.albedo, 1.0);
}
