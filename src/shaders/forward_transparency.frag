#version 330 core

in vec4 FragPos;

out vec4 FragColor;

layout (std140) uniform MaterialParams
{
    vec4 tintAndOpacity;
};

void main()
{
    FragColor = tintAndOpacity;
}
