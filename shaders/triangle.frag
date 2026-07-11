#version 450

layout(location = 0) in vec3 in_color;
layout(location = 1) in vec2 in_texture_coordinate;
layout(location = 0) out vec4 out_color;

void main()
{
    out_color = vec4(in_texture_coordinate, 0.25, 1.0);
}
