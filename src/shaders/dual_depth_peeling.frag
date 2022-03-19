#version 330 core
layout (location = 0) out vec2 minMaxDepth;
layout (location = 1) out vec4 frontBlender;
layout (location = 2) out vec4 tempBackBlender;

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

layout (std140) uniform CameraParams
{
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
    vec4 cameraPos;
    vec4 nearFarPlanes;
};

layout (std140) uniform Lighting
{
    mat4 directionalLightViewProjection;
    vec4 directionalLightColor;
    vec4 directionalLightDir;

    vec4 directionalBiasAndAngleBias;
};

uniform sampler2D shadow_map;

layout (std140) uniform MaterialParams
{
    vec4 tintAndOpacity;
    vec4 specularitySpecularStrDoShadingIsParticle;
};

uniform sampler2D previousDepthBlender;
uniform sampler2D previousFrontBlender;

#define MAX_DEPTH 1.f

vec3 calculateDiffuse(vec3 color, vec3 normal, vec3 lightDir);
//vec3 calculateSpecular(vec3 color, vec3 pos, vec3 normal, vec3 cameraPos, vec3 lightDir, float specularity, float specularPower);
vec3 calculateSpecular(vec3 color, vec3 pos, vec3 normal, vec3 cameraPos, vec3 lightDir, float specularity);
vec3 composeColor(float ambientIntensity, float shadowIntensity, vec3 ambientColor, vec3 diffuse, vec3 specular);

vec3 shade(vec3 pos, vec3 color, vec3 normal, float specularity, float specularStrength, vec2 uv);
vec3 gammaCorrect(vec3 color, float gamma);

vec4 under(vec4 backColor, vec4 frontColor)
{
    vec4 blendedColor;
    blendedColor.rgb = frontColor.rgb + backColor.rgb * backColor.a * (1.f - frontColor.a);
    blendedColor.a = frontColor.a + backColor.a * (1.f - frontColor.a);

    return blendedColor;
}

void main()
{
    float fragDepth = gl_FragCoord.z;

    vec2 uv = gl_FragCoord.xy / vec2(viewportWidth, viewportHeight);

    vec2 previousDepth = texture(previousDepthBlender, uv).xy;
    vec4 previousFront = texture(previousFrontBlender, uv);

    minMaxDepth = vec2(-MAX_DEPTH);
    frontBlender = previousFront;
    tempBackBlender = vec4(0.0);

    float nearestDepth = -previousDepth.x;
    float farthestDepth = previousDepth.y;
    float alphaMultiplier = 1.0 - previousFront.a;

    if (fragDepth < nearestDepth || fragDepth > farthestDepth) 
    {
        return;
    }

    if (fragDepth > nearestDepth && fragDepth < farthestDepth) 
    {
        minMaxDepth = vec2(-fragDepth, fragDepth);
        return;
    }

    //vec3 diffuse = calculateDiffuse(tintAndOpacity.rgb, Normal, directionalLightDir.xyz);
    //vec3 specular = calculateSpecular(tintAndOpacity.rgb, Pos, Normal, cameraPos.xyz, directionalLightDir.xyz, 10.f);
    //vec4 color = vec4(composeColor(0.1f, 0.f, tintAndOpacity.rgb, diffuse, specular), tintAndOpacity.a);
    //vec4 color = tintAndOpacity;
    minMaxDepth = vec2(-MAX_DEPTH);

    vec4 color = vec4(gammaCorrect(shade(Pos, tintAndOpacity.rgb, Normal, 256.f, 5.f, uv), gamma), tintAndOpacity.a);

    if (fragDepth == nearestDepth) 
    {
        frontBlender = under(color, previousFront);
        //frontBlender.xyz += color.rgb * color.a * alphaMultiplier;
        //frontBlender.w = 1.0 - alphaMultiplier * (1.0 - color.a);
    } 
    else 
    {
        tempBackBlender = color;
        //tempBackBlender = tintAndOpacity;
    }
}
