#version 450

layout(location = 0) in vec2 uv;
layout(location = 1) in vec3 tint;
layout(location = 2) in vec3 worldNormal;
layout(location = 0) out vec4 outColor;
layout(set = 0, binding = 0) uniform sampler2D testTexture;

void main() {
    vec4 texel = texture(testTexture, uv);
    vec3 normal = normalize(worldNormal);
    vec3 lightDirection = normalize(vec3(-0.45, 0.75, 0.55));
    float diffuse = max(dot(normal, lightDirection), 0.0);
    float ambient = 0.28;
    float lighting = ambient + diffuse * 0.72;
    vec3 litColor = texel.rgb * tint * lighting;
    outColor = vec4(litColor, texel.a);
}
