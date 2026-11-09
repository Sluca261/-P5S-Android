#version 450

layout(push_constant) uniform Camera {
    mat4 viewProjection;
} camera;

layout(location = 0) out vec2 uv;
layout(location = 1) out vec3 tint;

const vec3 positions[6] = vec3[](
    vec3(0.0, -0.62, 0.75),
    vec3(0.72, 0.58, 0.75),
    vec3(-0.72, 0.58, 0.75),
    vec3(0.0, -0.28, 0.15),
    vec3(0.38, 0.32, 0.15),
    vec3(-0.38, 0.32, 0.15)
);

const vec2 coordinates[6] = vec2[](
    vec2(0.5, 1.0), vec2(1.0, 0.0), vec2(0.0, 0.0),
    vec2(0.5, 1.0), vec2(1.0, 0.0), vec2(0.0, 0.0)
);

void main() {
    int index = gl_VertexIndex;
    gl_Position = camera.viewProjection * vec4(positions[index], 1.0);
    uv = coordinates[index];
    tint = index < 3 ? vec3(0.45, 0.75, 1.0) : vec3(1.0, 0.42, 0.72);
}
