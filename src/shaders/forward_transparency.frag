#version 330 core

in vec4 FragPos;

out vec4 FragColor;

layout (std140) uniform MaterialParams
{
    vec4 tintAndOpacity;
};

float visibleDepth(float depth, float near, float far);

void main()
{
    FragColor = tintAndOpacity;
    //FragColor = vec4(vec3(visibleDepth(FragPos.z/FragPos.w)), tintAndOpacity.w);
    //FragColor = vec4(vec3(visibleDepth(FragPos.z/FragPos.w, 0.1f, 100000.f)), tintAndOpacity.w);
}
