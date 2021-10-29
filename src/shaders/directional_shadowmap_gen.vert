#version 330 core
layout (location = 0) in vec3 vert_pos;
layout (location = 1) in vec3 vert_norm;
layout (location = 2) in vec3 vert_tan;
layout (location = 3) in vec2 vert_uv;

out vec4 FragPos;

layout (std140) uniform CameraParams
{
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
    vec4 cameraPos;
    vec4 nearFarPlanes;
};

layout (std140) uniform Lighting
{
    mat4 directionalLightViewProjection;
    vec4 directionalLightColor;
    vec4 directionalLightDir;

    vec4 directionalBiasAndAngleBias;
};

layout (std140) uniform ModelParams
{
    mat4 model;
};

void main()
{
    FragPos = model * vec4(vert_pos, 1.f);
    gl_Position = directionalLightViewProjection * model * vec4(vert_pos, 1.f);
}
