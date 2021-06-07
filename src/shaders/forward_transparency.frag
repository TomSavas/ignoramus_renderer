#version 330 core

in vec4 FragPos;

out vec4 FragColor;

uniform vec3 color;
uniform float opacity;

void main()
{
    FragColor = vec4(color, opacity);
}
