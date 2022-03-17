#version 330 core
layout (location = 0) in vec3 vert_pos;
layout (location = 1) in vec3 vert_norm;
layout (location = 2) in vec3 vert_tan;
layout (location = 3) in vec2 vert_uv;

out vec3 Pos;
out vec3 Normal;
out vec2 Uv;

layout (std140) uniform CameraParams
{
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
    vec4 cameraPos;
    vec4 nearFarPlanes;
};

layout (std140) uniform ModelParams
{
    mat4 model;
};

void main()
{
    Pos = (model * vec4(vert_pos, 1.f)).xyz;
    Uv = vert_uv;

    mat3 normalRecalculationMatrix = transpose(inverse(mat3(model)));
    Normal = normalize(normalRecalculationMatrix * vert_norm);

    gl_Position = viewProjection * model * vec4(vert_pos, 1.f);
}
