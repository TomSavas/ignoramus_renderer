#version 330 core

float linearizeDepth(float depth, float near, float far)
{
    float linearDepth = (2.f * near) / (far + near - depth * (far - near));
    return linearDepth;
}

float visibleDepth(float depth, float near, float far)
{
    const float visibilityScale = 700.f;
    return linearizeDepth(depth, near, far) * visibilityScale;
}

