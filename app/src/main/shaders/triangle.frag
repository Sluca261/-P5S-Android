#version 450

layout(location = 0) in vec2 uv;
layout(location = 1) in vec3 tint;
layout(location = 0) out vec4 outColor;
layout(set = 0, binding = 0) uniform sampler2D testTexture;

void main() {
    vec4 texel = texture(testTexture, uv);
    outColor = vec4(texel.rgb * tint, texel.a);
}
