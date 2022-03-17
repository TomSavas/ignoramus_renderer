#version 460 core
layout (location = 0) out vec4 accumulator;
layout (location = 1) out float revealage;

in vec3 Pos;
in vec3 Normal;

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

layout (std140) uniform MaterialParams
{
    vec4 tintAndOpacity;
};

vec3 shade(vec3 pos, vec3 color, vec3 normal, float specularity, float specularStrength, vec2 uv);

void main()
{
    vec2 fragmentPos = gl_FragCoord.xy / vec2(viewportWidth, viewportHeight);

    float alpha = tintAndOpacity.a;
    // weight function
    float weight = clamp(pow(min(1.0, alpha * 10.0) + 0.01, 3.0) * 1e8 * pow(1.0 - gl_FragCoord.z * 0.9, 3.0), 1e-2, 3e3);

    //vec3 color = tintAndOpacity.rgb;
    vec3 color = shade(Pos, tintAndOpacity.rgb, Normal, 256.f, 10.f, fragmentPos);
    // store pixel color accumulation
    accumulator = vec4(color * alpha, alpha) * weight;

    // store pixel revealage threshold
    revealage = alpha;
}
