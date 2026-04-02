#version 450
// M1 - Baseline triangle fragment shader
// Receives interpolated colour from the vertex shader and outputs
// it directly to the colour attachment. No lighting, no textures.

layout(location = 0) in  vec3 fragColor;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(fragColor, 1.0);
}
