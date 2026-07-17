#version 450

layout(location = 0) in vec3 in_color;
layout(location = 1) in vec2 in_texture_coordinate;
layout(location = 2) flat in uint in_texture_enabled;

layout(set = 0, binding = 0) uniform sampler2D texture_sampler;

layout(location = 0) out vec4 out_color;

void main()
{
    if (in_texture_enabled != 0u) {
        out_color = texture(texture_sampler, in_texture_coordinate);
    } else {
        out_color = vec4(in_color, 1.0);
    }
}
