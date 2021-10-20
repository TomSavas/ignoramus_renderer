#version 330 core

in vec4 FragPos;

out vec4 FragColor;

layout (std140) uniform MaterialParams
{
    vec4 tintAndOpacity;
};

float visibleDepth(float depth)
{
    float near = 0.1f;
    float far = 100000.f;

    depth = (2.f * near) / (far + near - depth * (far - near));
    return depth * 700;
}

void main()
{
    FragColor = tintAndOpacity;
    //FragColor = vec4(vec3(visibleDepth(FragPos.z/FragPos.w)), tintAndOpacity.w);
}
