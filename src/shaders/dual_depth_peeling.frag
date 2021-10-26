#version 330 core
layout (location = 0) out vec2 minMaxDepth;
layout (location = 1) out vec4 frontBlender;
layout (location = 2) out vec4 tempBackBlender;

layout (std140) uniform MaterialParams
{
    vec4 tintAndOpacity;
};

uniform sampler2D previousDepthBlender;
uniform sampler2D previousFrontBlender;

in vec2 uv;

#define MAX_DEPTH 1.f

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

    vec4 color = tintAndOpacity;
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
