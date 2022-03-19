#version 330 core

out vec4 FragColor;

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
vec3 gammaCorrect(vec3 color, float gamma);

void main()
{
    vec2 fragmentPos = gl_FragCoord.xy / vec2(viewportWidth, viewportHeight);
    
    vec4 color = tintAndOpacity;
    if (specularitySpecularStrDoShadingIsParticle.w > 0)
    {
        color = color * texture(particle_tex, Uv);
    }

    if (specularitySpecularStrDoShadingIsParticle.z <= 0)
    {
        FragColor = vec4(gammaCorrect(color.rgb, gamma), color.a);
    }
    else
    {
        float specularity = specularitySpecularStrDoShadingIsParticle.x;
        float specularStrength = specularitySpecularStrDoShadingIsParticle.y;
        FragColor = vec4(gammaCorrect(shade(Pos, color.rgb, Normal, specularity, specularStrength, fragmentPos), gamma), color.a);
    }

    // Good for showing sorting issues with unsorted forward transparency
    //FragColor = vec4(vec3(visibleDepth(gl_FragCoord.z, 1.0f, 100000.f)), tintAndOpacity.w);
}
