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
};

uniform sampler2D previousDepthBlender;
uniform sampler2D previousFrontBlender;

#define MAX_DEPTH 1.f

vec3 calculateDiffuse(vec3 color, vec3 normal, vec3 lightDir);
vec3 calculateSpecular(vec3 color, vec3 pos, vec3 normal, vec3 cameraPos, vec3 lightDir, float specularity, float specularPower);
vec3 composeColor(float ambientIntensity, float shadowIntensity, vec3 ambientColor, vec3 diffuse, vec3 specular);

void main()
{
    float fragDepth = gl_FragCoord.z;

    vec2 previousDepth = texture(previousDepthBlender, gl_FragCoord.xy / vec2(1920.f, 1080.f)).xy;
    vec4 previousFront = texture(previousFrontBlender, gl_FragCoord.xy / vec2(1920.f, 1080.f));

    minMaxDepth = vec2(-MAX_DEPTH);
    frontBlender = previousFront;
    tempBackBlender = vec4(0.0);

    float nearestDepth = -previousDepth.x;
    float farthestDepth = previousDepth.y;
    float alphaMultiplier = 1.0 - previousFront.w;

    if (fragDepth < nearestDepth || fragDepth > farthestDepth) 
    {
        return;
    }

    if (fragDepth > nearestDepth && fragDepth < farthestDepth) 
    {
        minMaxDepth = vec2(-fragDepth, fragDepth);
        return;
    }

    vec3 diffuse = calculateDiffuse(tintAndOpacity.rgb, Normal, directionalLightDir.xyz);
    vec3 specular = calculateSpecular(tintAndOpacity.rgb, Pos, Normal, cameraPos.xyz, directionalLightDir.xyz, 10.f, specularPower);
    vec4 color = vec4(composeColor(0.1f, 0.f, tintAndOpacity.rgb, diffuse, specular), tintAndOpacity.a);
    //vec4 color = tintAndOpacity;
    minMaxDepth = vec2(-MAX_DEPTH);

    if (fragDepth == nearestDepth) 
    {
        frontBlender.xyz += color.rgb * color.a * alphaMultiplier;
        frontBlender.w = 1.0 - alphaMultiplier * (1.0 - color.a);
    } 
    else 
    {
        tempBackBlender += color;
    }
}
