#version 450

layout(location = 0) in vec2 fragLocal;
layout(location = 1) in vec4 fragColorOpacity;
layout(location = 0) out vec4 outColor;

void main() {
    float distanceSquared = dot(fragLocal, fragLocal);
    if (distanceSquared > 1.0) {
        discard;
    }

    // A simple 2D Gaussian falloff makes each point appear as a soft splat
    // instead of a hard square billboard.
    float gaussian = exp(-4.0 * distanceSquared);
    float alpha = clamp(fragColorOpacity.a * gaussian, 0.0, 1.0);

    outColor = vec4(fragColorOpacity.rgb, alpha);
}
