#version 330 core

out vec4 FragColor;

in vec3 Pos;
in vec3 Normal;

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

vec3 shade(vec3 pos, vec3 color, vec3 normal, float specularity, float specularStrength, vec2 uv);
vec3 gammaCorrect(vec3 color, float gamma);

void main()
{
    vec2 fragmentPos = gl_FragCoord.xy / vec2(viewportWidth, viewportHeight);
    FragColor = vec4(gammaCorrect(shade(Pos, tintAndOpacity.rgb, Normal, 256.f, 10.f, fragmentPos), gamma), tintAndOpacity.a);
    //FragColor = vec4(gammaCorrect(shade(Pos, tintAndOpacity.rgb, Normal, 256.f, 10.f, fragmentPos), gamma), 1.f);

    // Good for showing sorting issues with unsorted forward transparency
    //FragColor = vec4(vec3(visibleDepth(gl_FragCoord.z, 1.0f, 100000.f)), tintAndOpacity.w);
}
