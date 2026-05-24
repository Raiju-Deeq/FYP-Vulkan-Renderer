#version 450

struct GaussianPoint {
    vec4 positionOpacity; // xyz = world position, w = opacity
    vec4 colorRadius;     // rgb = colour, w = base radius
};

layout(set = 0, binding = 0, std430) readonly buffer SplatBuffer {
    GaussianPoint points[];
} splats;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec4 controls; // x = radius scale, y = opacity scale
} pc;

layout(location = 0) out vec2 fragLocal;
layout(location = 1) out vec4 fragColorOpacity;

void main() {
    const vec2 corners[6] = vec2[](
        vec2(-1.0, -1.0),
        vec2( 1.0, -1.0),
        vec2( 1.0,  1.0),
        vec2(-1.0, -1.0),
        vec2( 1.0,  1.0),
        vec2(-1.0,  1.0)
    );

    GaussianPoint point = splats.points[gl_InstanceIndex];
    vec2 local = corners[gl_VertexIndex];

    // This first C3 pass uses screen-facing X/Y billboards.  It is enough to
    // prove .ply ingestion and soft splat rendering while keeping full 3DGS
    // covariance projection as future work.
    float radius = point.colorRadius.w * pc.controls.x;
    vec3 worldPosition = point.positionOpacity.xyz + vec3(local * radius, 0.0);

    gl_Position = pc.mvp * vec4(worldPosition, 1.0);
    fragLocal = local;
    fragColorOpacity = vec4(point.colorRadius.rgb,
                            point.positionOpacity.w * pc.controls.y);
}
