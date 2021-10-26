#version 330 core

out vec2 FragColor;

void main()
{
    FragColor = vec2(-gl_FragCoord.z, gl_FragCoord.z);
}
