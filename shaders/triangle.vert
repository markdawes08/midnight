#version 450

layout(location = 0) in vec2 in_position;
layout(location = 1) in vec3 in_color;
layout(location = 2) in vec2 in_texture_coordinate;
layout(location = 3) in uint in_texture_enabled;

layout(location = 0) out vec3 out_color;
layout(location = 1) out vec2 out_texture_coordinate;
layout(location = 2) flat out uint out_texture_enabled;

void main()
{
    gl_Position = vec4(in_position, 0.0, 1.0);
    out_color = in_color;
    out_texture_coordinate = in_texture_coordinate;
    out_texture_enabled = in_texture_enabled;
}
