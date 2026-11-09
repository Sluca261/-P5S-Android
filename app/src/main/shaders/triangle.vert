#version 450

layout(push_constant) uniform Camera {
    mat4 viewProjection;
    mat4 model;
} camera;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inTint;
layout(location = 3) in vec3 inNormal;

layout(location = 0) out vec2 uv;
layout(location = 1) out vec3 tint;
layout(location = 2) out vec3 worldNormal;

void main() {
    gl_Position = camera.viewProjection * vec4(inPosition, 1.0);
    uv = inUV;
    tint = inTint;
    worldNormal = normalize(mat3(camera.model) * inNormal);
}
