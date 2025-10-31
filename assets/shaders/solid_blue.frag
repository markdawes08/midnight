#version 450
layout(location = 0) in vec2 vUV;      // not used, but interface matches VS
layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(0.20, 0.40, 1.00, 1.0);  // blue
}
