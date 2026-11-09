#version 450

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;

void main() {
    // Padrão quadriculado procedural para validar coordenadas UV e fragmentos.
    vec2 cells = floor(uv * 8.0);
    float checker = mod(cells.x + cells.y, 2.0);
    vec3 darkTone = vec3(0.12, 0.025, 0.16);
    vec3 brightTone = vec3(0.95, 0.12, 0.48);
    vec3 color = mix(darkTone, brightTone, checker);
    outColor = vec4(color, 1.0);
}
