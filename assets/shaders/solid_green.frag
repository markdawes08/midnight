#version 450
layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

void main() {
    // simple checker
    float c = mod(floor(vUV.x) + floor(vUV.y), 2.0);
    vec3 a = vec3(0.20, 0.80, 0.30);
    vec3 b = vec3(0.15, 0.65, 0.25);
    outColor = vec4(mix(a, b, c), 1.0);
}
