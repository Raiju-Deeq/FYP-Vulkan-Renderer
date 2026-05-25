#version 450

layout(set = 0, binding = 0) uniform sampler2D baseColorTexture;
layout(set = 0, binding = 2) uniform sampler2D metallicRoughnessTexture;
layout(set = 0, binding = 3) uniform sampler2D aoTexture;
layout(set = 0, binding = 4) uniform sampler2D emissiveTexture;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    // Packed material/light inputs from C++.  I use vec4 groups because they
    // are simple to align between C++ and GLSL push constants.
    vec4 baseColorFactor;         // rgb = baseColor factor
    vec4 debugOptions;            // x = debug mode
    vec4 lightDirectionIntensity; // xyz = direct light direction, w = intensity
} pc;

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 fragNormal;
layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359;

// GGX / Trowbridge-Reitz normal distribution function.
// This estimates how many surface microfacets are aligned with the halfway
// vector.  Low roughness gives a tight highlight; high roughness spreads it out.
float distributionGGX(vec3 normal, vec3 halfway, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float nDotH = max(dot(normal, halfway), 0.0);
    float nDotH2 = nDotH * nDotH;

    float denominator = (nDotH2 * (a2 - 1.0) + 1.0);
    denominator = PI * denominator * denominator;
    return a2 / max(denominator, 0.0001);
}

// Schlick-GGX geometry term for one direction.
// This approximates microfacet self-shadowing: rough surfaces hide more of
// their own tiny facets, so less reflected light reaches the camera.
float geometrySchlickGGX(float nDotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return nDotV / max(nDotV * (1.0 - k) + k, 0.0001);
}

// Smith combines the geometry term for both the view direction and light
// direction.  Both directions matter because light must reach the surface and
// then bounce from the surface to the camera.
float geometrySmith(vec3 normal, vec3 viewDir, vec3 lightDir, float roughness) {
    float nDotV = max(dot(normal, viewDir), 0.0);
    float nDotL = max(dot(normal, lightDir), 0.0);
    float ggxView = geometrySchlickGGX(nDotV, roughness);
    float ggxLight = geometrySchlickGGX(nDotL, roughness);
    return ggxView * ggxLight;
}

// Fresnel-Schlick estimates how reflective the surface is at this angle.
// Surfaces reflect more strongly at grazing angles.  F0 is the base reflectance:
// dielectrics sit around 0.04, while metals use their albedo as reflectance.
vec3 fresnelSchlick(float cosTheta, vec3 f0) {
    return f0 + (1.0 - f0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main() {
    // "Normal" is one of the expected material inputs, but this prototype does
    // not apply tangent-space normal maps yet.  The descriptor slot and ImGui
    // upload button exist so I can load a normal map with the rest of a PBR set,
    // but shading still uses the imported/interpolated vertex normal until the
    // mesh format grows tangents and bitangents.
    vec3 normal = normalize(fragNormal);

    // S2 normals view: remap normal components from [-1, 1] into visible RGB.
    if (pc.debugOptions.x > 0.5) {
        outColor = vec4(normal * 0.5 + 0.5, 1.0);
        return;
    }

    // Disney-style material input summary for this milestone:
    // - baseColor = current base-colour texture multiplied by a colour factor.
    // - metallic/roughness = packed texture values.
    // - normal = vertex normal for now; uploaded normal map is reserved.
    // - AO = texture red channel.
    // - emissive = emissive texture colour.
    //
    // The base-colour image uses VK_FORMAT_R8G8B8A8_SRGB, so Vulkan converts the
    // sampled value into linear colour before the shader receives it.
    vec3 baseColorFactor = clamp(pc.baseColorFactor.rgb, vec3(0.0), vec3(1.0));
    vec3 albedo = texture(baseColorTexture, fragTexCoord).rgb * baseColorFactor;

    vec4 metallicRoughnessSample = texture(metallicRoughnessTexture, fragTexCoord);
    float roughnessMap = metallicRoughnessSample.g;
    float metallicMap = metallicRoughnessSample.b;
    float aoMap = texture(aoTexture, fragTexCoord).r;
    vec3 emissiveMap = texture(emissiveTexture, fragTexCoord).rgb;

    float metallic = clamp(metallicMap, 0.0, 1.0);
    float roughness = clamp(roughnessMap, 0.04, 1.0);
    float ambientOcclusion = clamp(aoMap, 0.0, 1.0);
    vec3 emissive = max(emissiveMap, vec3(0.0));

    // Direct lighting with Cook-Torrance BRDF.  The light direction is a
    // runtime input, so I can move it around the model and see how
    // roughness/metallic values react.
    vec3 lightDir = normalize(pc.lightDirectionIntensity.xyz);
    vec3 viewDir = normalize(vec3(0.0, 0.0, 1.0));
    vec3 halfway = normalize(viewDir + lightDir);
    vec3 radiance = vec3(max(pc.lightDirectionIntensity.w, 0.0));

    vec3 f0 = mix(vec3(0.04), albedo, metallic);
    float ndf = distributionGGX(normal, halfway, roughness);
    float geometry = geometrySmith(normal, viewDir, lightDir, roughness);
    vec3 fresnel = fresnelSchlick(max(dot(halfway, viewDir), 0.0), f0);

    vec3 numerator = ndf * geometry * fresnel;
    float denominator = 4.0 *
        max(dot(normal, viewDir), 0.0) *
        max(dot(normal, lightDir), 0.0);
    vec3 specular = numerator / max(denominator, 0.0001);

    // Energy conservation: if more energy goes into specular reflection, less
    // remains for diffuse.  Metals have no diffuse term in this simple model.
    vec3 kS = fresnel;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

    float nDotL = max(dot(normal, lightDir), 0.0);
    vec3 directLighting = (kD * albedo / PI + specular) * radiance * nDotL;
    vec3 ambient = albedo * 0.03 * ambientOcclusion;

    vec3 colour = ambient + directLighting + emissive;

    // Simple Reinhard tone mapping keeps bright specular highlights from
    // clipping too harshly.  I leave the result in linear colour because the
    // swapchain attachment is sRGB, so Vulkan performs the final display
    // conversion when the colour is written.
    colour = colour / (colour + vec3(1.0));

    outColor = vec4(colour, 1.0);
}
