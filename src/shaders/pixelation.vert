#version 330 core
layout (location = 0) in vec3 vert_pos;
layout (location = 1) in vec3 vert_norm;
layout (location = 2) in vec3 vert_tan;
layout (location = 3) in vec2 vert_uv;

out vec2 uv;

void main()
{
    // Only a quad so directly transfer NDC coords
    gl_Position = vec4(vert_pos, 1.f);
    uv = vert_uv;
}
