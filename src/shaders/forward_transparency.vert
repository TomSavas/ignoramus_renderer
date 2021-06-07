#version 330 core
layout (location = 0) in vec3 gPosition;
layout (location = 1) in vec3 gNormal;
layout (location = 2) in vec4 gAlbedo;
layout (location = 3) in vec3 gSpec;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec4 FragPos;

void main()
{
    FragPos = projection * view * model * vec4(gPosition, 1.f);
    gl_Position = FragPos;
}
