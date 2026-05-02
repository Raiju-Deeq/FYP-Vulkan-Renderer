#version 450

layout(set = 0, binding = 0) uniform sampler2D albedoTexture;

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 fragNormal;
layout(location = 0) out vec4 outColor;

void main() {
    vec3 normal = normalize(fragNormal);
    vec3 lightDir = normalize(vec3(0.4, 0.8, 0.6));
    float diffuse = max(dot(normal, lightDir), 0.0);
    vec3 albedo = texture(albedoTexture, fragTexCoord).rgb;
    outColor = vec4(albedo * (0.25 + diffuse * 0.75), 1.0);
}
