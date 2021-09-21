#version 330 core
layout (location = 0) in vec3 gPosition;
layout (location = 1) in vec3 gNormal;
layout (location = 2) in vec4 gAlbedo;
layout (location = 3) in vec3 gSpec;

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

void main()
{
    FragPos = projection * view * model * vec4(gPosition, 1.f);
    gl_Position = FragPos;
}
