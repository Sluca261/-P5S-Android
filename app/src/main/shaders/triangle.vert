#version 450

layout(location = 0) out vec2 uv;

const vec3 positions[3] = vec3[](
    vec3(0.0, -0.62, 0.25),
    vec3(0.72, 0.58, 0.0),
    vec3(-0.72, 0.58, 0.0)
);

const vec2 coordinates[3] = vec2[](
    vec2(0.5, 1.0),
    vec2(1.0, 0.0),
    vec2(0.0, 0.0)
);

void main() {
    vec3 position = positions[gl_VertexIndex];
    float perspective = 1.0 + position.z * 0.35;
    gl_Position = vec4(position.xy, position.z, perspective);
    uv = coordinates[gl_VertexIndex];
}
