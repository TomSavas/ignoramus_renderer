#version 460
layout (location = 0) out vec3 fragColor;

layout (std140) uniform SceneParams
{
    int pixelSize;
    bool wireframe;
    bool useDepthLightCullingOptimisation;
    float gamma;
    float specularPower;
    float viewportWidth;
    float viewportHeight;
};

vec3 gammaCorrect(vec3 color, float gamma);
vec3 shadeFromTex(vec2 uv);

void main()
{
    vec2 fragmentPos = gl_FragCoord.xy / vec2(viewportWidth, viewportHeight);
    fragColor = gammaCorrect(shadeFromTex(fragmentPos), gamma);
}
