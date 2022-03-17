#version 460 core
layout (location = 0) out vec4 accumulator;
layout (location = 1) out float revealage;

in vec3 Pos;
in vec3 Normal;
in vec2 Uv;

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
    ivec4 doShadingIsParticle;
};
uniform sampler2D particle_tex;

vec3 shade(vec3 pos, vec3 color, vec3 normal, float specularity, float specularStrength, vec2 uv);

void main()
{
    vec2 fragmentPos = gl_FragCoord.xy / vec2(viewportWidth, viewportHeight);

    vec4 particleTextureColor = vec4(1.f);
    if (doShadingIsParticle.y != 0)
    {
        particleTextureColor = texture(particle_tex, Uv);
    }
    vec4 color = tintAndOpacity * particleTextureColor;

    float alpha = color.a;
    // weight function
    float weight = clamp(pow(min(1.0, alpha * 10.0) + 0.01, 3.0) * 1e8 * pow(1.0 - gl_FragCoord.z * 0.9, 3.0), 1e-2, 3e3);

    if (doShadingIsParticle.x != 0)
    {
        color = vec4(shade(Pos, color.rgb, Normal, 256.f, 10.f, fragmentPos), color.a);
    }

    // store pixel color accumulation
    accumulator = vec4(color.rgb * alpha, alpha) * weight;

    // store pixel revealage threshold
    revealage = alpha;
}
