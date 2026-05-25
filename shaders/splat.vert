#version 450

// ============================================================
// splat.vert
// Projects source Gaussian records on the GPU and expands each one into a
// screen-space quad.
//
// This replaces the slow CPU path where I projected, sorted and uploaded splat
// records every frame.  The CPU now uploads the PLY once; this shader does the
// per-frame camera work in parallel across the GPU.
// ============================================================

struct GaussianPoint {
    vec4 positionOpacity; // xyz = model-space position, w = activated opacity
    vec4 color;           // rgb = base colour, w = padding
    vec4 covarianceA;     // xx, xy, xz, yy
    vec4 covarianceB;     // yz, zz, padding, padding
};

layout(set = 0, binding = 0, std430) readonly buffer GaussianBuffer {
    GaussianPoint points[];
} gaussianBuffer;

layout(push_constant) uniform PushConstants {
    mat4 modelView;        // model space -> view space
    vec4 viewportFocal;    // x = width, y = height, z = focalX, w = focalY
    vec4 projectionParams; // x = tanHalfFovX, y = tanHalfFovY, z = near, w = far
    vec4 controls;         // x = radius scale, y = opacity scale
} pc;

layout(location = 0) out vec2 fragPixelOffset;
layout(location = 1) out vec3 fragColor;
layout(location = 2) out float fragOpacity;
layout(location = 3) out vec3 fragConic;

const vec2 QUAD[6] = vec2[](
    vec2(-1.0, -1.0),
    vec2( 1.0, -1.0),
    vec2(-1.0,  1.0),
    vec2(-1.0,  1.0),
    vec2( 1.0, -1.0),
    vec2( 1.0,  1.0)
);

void hideSplat() {
    gl_Position = vec4(2.0, 2.0, 1.0, 1.0);
    fragPixelOffset = vec2(0.0);
    fragColor = vec3(0.0);
    fragOpacity = 0.0;
    fragConic = vec3(1.0, 0.0, 1.0);
}

void main() {
    GaussianPoint gaussian = gaussianBuffer.points[gl_InstanceIndex];
    vec4 viewPosition4 = pc.modelView * vec4(gaussian.positionOpacity.xyz, 1.0);
    vec3 viewPosition = viewPosition4.xyz;
    float positiveDepth = -viewPosition.z;

    // I cull splats behind the camera in the vertex shader by moving their
    // quad outside clip space.  This avoids fragment work without needing CPU
    // visibility filtering.
    if (positiveDepth < pc.projectionParams.z) {
        hideSplat();
        return;
    }

    float width = pc.viewportFocal.x;
    float height = pc.viewportFocal.y;
    float focalX = pc.viewportFocal.z;
    float focalY = pc.viewportFocal.w;
    float tanHalfFovX = pc.projectionParams.x;
    float tanHalfFovY = pc.projectionParams.y;

    float ndcX = viewPosition.x / (positiveDepth * tanHalfFovX);
    float ndcY = -viewPosition.y / (positiveDepth * tanHalfFovY);
    if (abs(ndcX) > 1.5 || abs(ndcY) > 1.5) {
        hideSplat();
        return;
    }

    vec2 screenPos = vec2((ndcX * 0.5 + 0.5) * width,
                          (ndcY * 0.5 + 0.5) * height);

    // I project the precomputed 3D covariance into a 2D screen-space conic.
    // This is the EWA-style projection step: I turn the 3D covariance into a
    // 2D screen-space ellipse so the fragment shader can evaluate the splat.
    float invZ = 1.0 / viewPosition.z;
    float txTz = clamp(viewPosition.x * invZ, -tanHalfFovX * 1.3, tanHalfFovX * 1.3);
    float tyTz = clamp(viewPosition.y * invZ, -tanHalfFovY * 1.3, tanHalfFovY * 1.3);

    vec3 jacobianRow0 = vec3(focalX * invZ, 0.0, -(focalX * txTz) * invZ);
    vec3 jacobianRow1 = vec3(0.0, focalY * invZ, -(focalY * tyTz) * invZ);

    vec3 viewRow0 = vec3(pc.modelView[0][0], pc.modelView[1][0], pc.modelView[2][0]);
    vec3 viewRow1 = vec3(pc.modelView[0][1], pc.modelView[1][1], pc.modelView[2][1]);
    vec3 viewRow2 = vec3(pc.modelView[0][2], pc.modelView[1][2], pc.modelView[2][2]);

    // T = J * V.  I keep the two T rows as vec3s because that is clearer than
    // fighting GLSL's column-major matrix constructor rules here.
    vec3 t0 = jacobianRow0.x * viewRow0 +
              jacobianRow0.y * viewRow1 +
              jacobianRow0.z * viewRow2;
    vec3 t1 = jacobianRow1.x * viewRow0 +
              jacobianRow1.y * viewRow1 +
              jacobianRow1.z * viewRow2;

    float cov00 = gaussian.covarianceA.x;
    float cov01 = gaussian.covarianceA.y;
    float cov02 = gaussian.covarianceA.z;
    float cov11 = gaussian.covarianceA.w;
    float cov12 = gaussian.covarianceB.x;
    float cov22 = gaussian.covarianceB.y;

    float covXx =
        t0.x * cov00 * t0.x + t0.y * cov11 * t0.y + t0.z * cov22 * t0.z +
        2.0 * (t0.x * cov01 * t0.y +
               t0.x * cov02 * t0.z +
               t0.y * cov12 * t0.z);
    float covXy =
        t0.x * cov00 * t1.x + t0.y * cov11 * t1.y + t0.z * cov22 * t1.z +
        t0.x * cov01 * t1.y + t0.y * cov01 * t1.x +
        t0.x * cov02 * t1.z + t0.z * cov02 * t1.x +
        t0.y * cov12 * t1.z + t0.z * cov12 * t1.y;
    float covYy =
        t1.x * cov00 * t1.x + t1.y * cov11 * t1.y + t1.z * cov22 * t1.z +
        2.0 * (t1.x * cov01 * t1.y +
               t1.x * cov02 * t1.z +
               t1.y * cov12 * t1.z);

    covXx += 0.3;
    covYy += 0.3;

    float determinant = covXx * covYy - covXy * covXy;
    if (determinant <= 0.0) {
        hideSplat();
        return;
    }

    float invDet = 1.0 / determinant;
    vec3 conic = vec3(covYy * invDet, -covXy * invDet, covXx * invDet);

    float mid = 0.5 * (covXx + covYy);
    float disc = max(0.1, mid * mid - determinant);
    float lambda1 = mid + sqrt(disc);
    float radius = min(ceil(3.0 * sqrt(lambda1)) * pc.controls.x, 1024.0);

    vec2 corner = QUAD[gl_VertexIndex % 6];
    vec2 pixelOffset = corner * radius;
    vec2 ndc = (screenPos + pixelOffset) / vec2(width, height) * 2.0 - vec2(1.0);

    float nearPlane = pc.projectionParams.z;
    float farPlane = pc.projectionParams.w;
    float ndcDepth = clamp((positiveDepth - nearPlane) / (farPlane - nearPlane), 0.0, 1.0);

    gl_Position = vec4(ndc.x, ndc.y, ndcDepth, 1.0);
    fragPixelOffset = pixelOffset;
    fragColor = gaussian.color.rgb;
    fragOpacity = clamp(gaussian.positionOpacity.w * pc.controls.y, 0.0, 0.99);
    fragConic = conic;
}
