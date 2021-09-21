#version 330 core

in vec4 FragPos;

out vec4 FragColor;

layout (std140) uniform MaterialParams
{
    float opacity;
    float r;
    float g;
    float b;
};

void main()
{
    FragColor = vec4(r, g, b, opacity);
    //FragColor = vec4(1.f, 0.f, 0.f, 1.f);
}
