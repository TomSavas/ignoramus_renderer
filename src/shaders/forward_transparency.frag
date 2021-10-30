#version 330 core

out vec4 FragColor;

layout (std140) uniform MaterialParams
{
    vec4 tintAndOpacity;
};

float linearizeDepth(float depth, float near, float far)
{
    float linearDepth = (2.f * near) / (far + near - depth * (far - near));
    return linearDepth;
}

float visibleDepth(float depth, float near, float far)
{
    return linearizeDepth(depth, near, far) * 20;
}

void main()
{
    FragColor = tintAndOpacity;
    FragColor = vec4(vec3(visibleDepth(gl_FragCoord.z, 1.0f, 100000.f)), tintAndOpacity.w);
}
