#version 450
layout(push_constant) uniform Push { mat4 mvp; } pc;
layout(location = 0) in vec3 in_pos;

// Varying to fragment
layout(location = 0) out vec2 vUV;

void main() {
    vUV = in_pos.xz * 0.1;       // scale for tile size
    gl_Position = pc.mvp * vec4(in_pos, 1.0);
}

