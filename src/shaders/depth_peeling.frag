#version 330 core
in vec3 Pos;
in vec3 Normal;

out vec4 FragColor;

layout (std140) uniform SceneParams
{
    int pixelSize;
    bool wireframe;
    float gamma;
    float specularPower;
    float viewportWidth;
    float viewportHeight;
};

layout (std140) uniform MaterialParams
{
    vec4 tintAndOpacity;
};

uniform sampler2D greater_depth;

vec3 shade(vec3 pos, vec3 color, vec3 normal, float specularity, float specularStrength, vec2 uv);
vec3 gammaCorrect(vec3 color, float gamma);

void main()
{
    vec2 uv = gl_FragCoord.xy / vec2(viewportWidth, viewportHeight);
    float z = gl_FragCoord.z;

    float minDepth = texture(greater_depth, uv).r;

    if (z <= minDepth)
    {
        discard;
    }

    FragColor = vec4(gammaCorrect(shade(Pos, tintAndOpacity.rgb, Normal, 256.f, 10.f, uv), gamma), tintAndOpacity.a);
}
