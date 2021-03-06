#version 460 core
layout (location = 0) out vec4 accumulator;
layout (location = 1) out float revealage;
layout (location = 2) out vec4 d;

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

layout (std140) uniform CameraParams
{
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
    vec4 cameraPos;
    vec4 nearFarPlanes;
};

layout (std140) uniform MaterialParams
{
    vec4 tintAndOpacity;
    vec4 specularitySpecularStrDoShadingIsParticle;
};
uniform sampler2D particle_tex;

layout (binding = transparencyDepth_AUTO_BINDING, r32ui) uniform uimage2D transparencyDepth;

vec3 shade(vec3 pos, vec3 color, vec3 normal, float specularity, float specularStrength, vec2 uv);
float linearizeDepthFromCameraParams(float depth);

void main()
{
    vec2 fragmentPos = gl_FragCoord.xy / vec2(viewportWidth, viewportHeight);

    vec4 particleTextureColor = vec4(1.f);
    if (specularitySpecularStrDoShadingIsParticle.w > 0)
    {
        particleTextureColor = texture(particle_tex, Uv);
    }
    vec4 color = tintAndOpacity * particleTextureColor;

    float alpha = color.a;
    // weight function
    float weight = clamp(pow(min(1.0, alpha * 10.0) + 0.01, 3.0) * 1e8 * pow(1.0 - gl_FragCoord.z * 0.9, 3.0), 1e-2, 3e3);

    //weight = max(min(1.0, max(max(color.r, color.g), color.b) * color.a), color.a) *
    //        clamp(0.03 / (1e-5 + pow(gl_FragCoord.z / 200, 4.0)), 1e-2, 3e3);
    //weight = alpha * pow(1.f - gl_FragCoord.z, 1.f);
    weight = pow(min(1.0, alpha * 10.0) + 0.01, 3.0) * pow(1.f - gl_FragCoord.z, 1.9f) * 1e9;

    if (specularitySpecularStrDoShadingIsParticle.z > 0)
    {
        float specularity = specularitySpecularStrDoShadingIsParticle.x;
        float specularStrength = specularitySpecularStrDoShadingIsParticle.y;
        color = vec4(shade(Pos, color.rgb, Normal, specularity, specularStrength, fragmentPos), color.a);
    }

    float depth = gl_FragCoord.z * nearFarPlanes.y;
    imageStore(transparencyDepth, ivec2(gl_FragCoord.xy), ivec4(depth));

    // store pixel color accumulation
    accumulator = vec4(color.rgb * alpha, alpha) * weight;

    // store pixel revealage threshold
    //revealage = min(alpha + doShadingIsParticleIsProxy.z, 1.f);
    revealage = alpha;
}
