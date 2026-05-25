#version 450

// ============================================================
// splat.frag
// Evaluates a 2D Gaussian conic and outputs pre-multiplied alpha.
// ============================================================

layout(location = 0) in vec2 fragPixelOffset;
layout(location = 1) in vec3 fragColor;
layout(location = 2) in float fragOpacity;
layout(location = 3) in vec3 fragConic;

layout(location = 0) out vec4 outColor;

void main() {
    vec2 d = fragPixelOffset;

    // 2D Gaussian exponent:
    // power = -0.5 * (a*dx^2 + 2*b*dx*dy + c*dy^2)
    float power = -0.5 * (
        fragConic.x * d.x * d.x +
        2.0 * fragConic.y * d.x * d.y +
        fragConic.z * d.y * d.y
    );

    if (power > 0.0) {
        discard;
    }

    float alpha = min(0.99, fragOpacity * exp(power));
    if (alpha < 1.0 / 255.0) {
        discard;
    }

    // Pipeline blending uses srcColor = ONE, so colour must be pre-multiplied.
    outColor = vec4(fragColor * alpha, alpha);
}
