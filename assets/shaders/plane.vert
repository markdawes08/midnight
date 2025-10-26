#version 450
layout(push_constant) uniform Push { mat4 mvp; } pc;
layout(location = 0) in vec3 in_pos;
void main() {
    gl_Position = pc.mvp * vec4(in_pos, 1.0);
}
