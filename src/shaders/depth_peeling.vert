#version 330 core
layout (location = 0) in vec3 vert_pos;
layout (location = 1) in vec3 vert_norm;
layout (location = 2) in vec3 vert_tan;
layout (location = 3) in vec2 vert_uv;

layout (std140) uniform CameraParams
{
    mat4 view;
    mat4 projection;
    vec3 cameraPos;
};

layout (std140) uniform ModelParams
{
    mat4 model;
};

out vec4 FragPos;
//out vec2 uv;

void main()
{
    FragPos = projection * view * model * vec4(vert_pos, 1.f);
    //uv = FragPos.xy / FragPos.w;
    gl_Position = FragPos;
}
