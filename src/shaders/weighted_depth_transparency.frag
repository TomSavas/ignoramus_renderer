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
    vec4 specularitySpecularStrDoShadingIsParticle;
};
uniform sampler2D particle_tex;

vec3 shade(vec3 pos, vec3 color, vec3 normal, float specularity, float specularStrength, vec2 uv);

void main()
{
    vec2 fragmentPos = gl_FragCoord.xy / vec2(viewportWidth, viewportHeight);

    vec4 particleTextureColor = vec4(1.f);
    if (specularitySpecularStrDoShadingIsParticle.w > 0)
    {
        particleTextureColor = texture(particle_tex, Uv);
    }
    vec4 color = tintAndOpacity * particleTextureColor;

    if (specularitySpecularStrDoShadingIsParticle.z > 0)
    {
        float specularity = specularitySpecularStrDoShadingIsParticle.x;
        float specularStrength = specularitySpecularStrDoShadingIsParticle.y;
        color = vec4(shade(Pos, color.rgb, Normal, specularity, specularStrength, fragmentPos), color.a);
    }

    float alpha = color.a;
    // weight function
    float weight;
    //weight = clamp(pow(min(1.0, alpha * 10.0) + 0.01, 3.0) * 1e8 * pow(1.0 - gl_FragCoord.z * 0.9, 5.0), 1e-2, 3e3);
    //weight = max(min(1.0, max(max(color.r, color.g), color.b) * color.a), color.a) *
    //        clamp(0.03 / (1e-5 + pow(gl_FragCoord.z / 3000, 4.0)), 1e-2, 3e3);
    //weight = pow(10.f + gl_FragCoord.z, 0.0);
    //weight = alpha * pow(1.f - gl_FragCoord.z, 1.3);
    //weight = pow(min(1.0, alpha * 10.0) + 0.01, 3.0) * pow(1.f - gl_FragCoord.z, 1.9f) * 1e9;
    weight = pow(min(1.0, alpha * 10.0) + 0.01, 3.0) * pow(1.f - gl_FragCoord.z, 1.9f) * 1e9;

    //float d = (100000.f * 1.f / (gl_FragCoord.z - 100000.f)) / (1.f - 100000.f);
    //weight = alpha * max(1e-2, 3e30 * pow(1.f - d, 7.f));

    // store pixel color accumulation
    accumulator = vec4(color.rgb * alpha, alpha) * weight;

    // store pixel revealage threshold
    //revealage = min(alpha + doShadingIsParticleIsProxy.z, 1.f);
    revealage = alpha;
}
