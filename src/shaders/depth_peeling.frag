#version 330 core
in vec4 FragPos;
//in vec2 uv;

out vec4 FragColor;

uniform sampler2D greater_depth;

layout (std140) uniform MaterialParams
{
    vec4 tintAndOpacity;
};

float visibleDepth(float depth)
{
    float near = 0.1f;
    float far = 100000.f;

    depth = (2.f * near) / (far + near - depth * (far - near));
    return depth * 20;
}

void main()
{
    vec2 uv = gl_FragCoord.xy / vec2(1920.f, 1080.f);
    float z = gl_FragCoord.z;

    float minDepth = texture(greater_depth, uv).r;

    if (z <= minDepth)
        discard;

    FragColor = tintAndOpacity;
}
