#version 460 core
out vec4 FragColor;

in vec2 uv;

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

uniform sampler2D accumulator;
uniform sampler2D revealage;

struct TransparencyData
{
    vec4 color;
    vec3 pos;
    vec3 normal;
    float depth;
    uint nextFragmentIndex;
};

layout (binding = ppllHeads_AUTO_BINDING, r32ui) uniform uimage2D ppllHeads;
layout (binding = transparentFragmentCount_AUTO_BINDING) uniform atomic_uint transparentFragmentCount;
layout (binding = TransparentFragments_AUTO_BINDING, std430) buffer TransparentFragments
{
    TransparencyData ppll[];
};

layout (binding = transparencyDepth_AUTO_BINDING, r32ui) uniform uimage2D transparencyDepth;

const float EPSILON = 0.00001f;

bool isApproximatelyEqual(float a, float b)
{
    return abs(a - b) <= (abs(a) < abs(b) ? abs(b) : abs(a)) * EPSILON;
}

float max3(vec3 v) 
{
    return max(max(v.x, v.y), v.z);
}

vec3 gammaCorrect(vec3 color, float gamma);

void main()
{
    float revealage = texture(revealage, uv).r;

    // save the blending and color texture fetch cost if there is not a transparent fragment
    if (isApproximatelyEqual(revealage, 1.0f)) 
    {
        discard;
        return;
    }

    vec4 accumulation = texture(accumulator, uv);

    // suppress overflow
    if (isinf(max3(abs(accumulation.rgb)))) 
        accumulation.rgb = vec3(accumulation.a);

    // prevent floating point precision bug
    vec3 average_color = accumulation.rgb / max(accumulation.a, EPSILON);

    //FragColor = vec4(gammaCorrect(average_color, gamma), 1.0f - revealage);

    const int maxTransparencyLayers = 16;
    const int linkedListSize = 1920 * 1080 * maxTransparencyLayers;
    if (atomicCounter(transparentFragmentCount) >= linkedListSize)
    {
        // TODO: try to replace the existing data with data closer to the camera
        return;
    }

    vec4 color = vec4(gammaCorrect(average_color, gamma), 1.0f - revealage);
    //vec4 color = vec4(vec3(1.f - revealage), 1.f);

    //float weightedBlendedDepth = float(imageLoad(transparencyDepth, ivec2(gl_FragCoord.xy)).r) / nearFarPlanes.y;
    float depth = float(imageLoad(transparencyDepth, ivec2(gl_FragCoord.xy)).r) / 100000.f;
    //depth = 1.f;
    //color = vec4(vec3(depth / 10.f), 1.f);

    FragColor = color;

    uint index = atomicCounterIncrement(transparentFragmentCount);
    uint lastHead = imageAtomicExchange(ppllHeads, ivec2(gl_FragCoord.xy), index);

    ppll[index].normal = vec3(1.f, 2.f, 3.f);
    ppll[index].color = color;
    ppll[index].pos = vec3(4.f, 5.f, 6.f);
    ppll[index].depth = depth;
    ppll[index].nextFragmentIndex = lastHead;

    // Add two layers, since we're always peeling in twos. A change could be made to use only a single layer.
    index = atomicCounterIncrement(transparentFragmentCount);
    lastHead = imageAtomicExchange(ppllHeads, ivec2(gl_FragCoord.xy), index);

    ppll[index].normal = vec3(1.f, 2.f, 3.f);
    ppll[index].color = color;
    ppll[index].pos = vec3(4.f, 5.f, 6.f);
    ppll[index].depth = depth;// * 1.1;
    ppll[index].nextFragmentIndex = lastHead;
}
