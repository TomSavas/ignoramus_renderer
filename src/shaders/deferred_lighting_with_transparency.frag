#version 430
layout (location = 0) out vec3 fragColor;

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

float linearizeDepthFromCameraParams(float depth);
vec3 gammaCorrect(vec3 color, float gamma);

vec4 under(vec4 backColor, vec4 frontColor)
{
    vec4 blendedColor;
    blendedColor.rgb = frontColor.rgb + backColor.rgb * backColor.a * (1.f - frontColor.a);
    blendedColor.a = frontColor.a + backColor.a * (1.f - frontColor.a);

    return blendedColor;
}

//vec4 over(vec4 frontColor, vec4 backColor)
//{
//    transparencyColor.rgb = transpColor * layers[i].color.a + transparencyColor.rgb * (1.f - layers[i].color.a);
//    transparencyColor.a = transparencyColor.a + layers[i].color.a * alpha;
//}

vec3 shade(vec3 pos, vec3 color, vec3 normal, float specularity, float specularStrength, vec2 uv);
vec3 shadeFromTex(vec2 uv);

vec4 traceTransparency()
{
#define MAX_TRANSPARENCY_LAYERS 32
#define NO_TRANSPARENCY_INDEX 0xffffffff
#define NO_TRANSPARENCY_COLOR vec4(-1.f)

    float lastDepth = 0;
    int layerCount = 0;
    vec4 color = vec4(0.f);
    vec3 rayEndPos;

    vec4 uv = vec4(gl_FragCoord.xy / vec2(1920.f, 1080.f), 0.f, 0.f);

    float thickness;
    while (layerCount < MAX_TRANSPARENCY_LAYERS)
    {
        uint head = imageLoad(ppllHeads, ivec2(uv.xy * vec2(1920.f, 1080.f))).r;
        if (head == NO_TRANSPARENCY_INDEX)
        {
            break;
            //return NO_TRANSPARENCY_COLOR;
        }

        // Look for transparency layer we hit
        TransparencyData frontLayer;
        TransparencyData backLayer;

        backLayer.nextFragmentIndex = head; // Just a fictional layer to start off the iteration
        do 
        {
            frontLayer = ppll[backLayer.nextFragmentIndex]; 
            if (frontLayer.nextFragmentIndex == NO_TRANSPARENCY_INDEX)
            {
                frontLayer.depth = 0.f; // Fake the front layer to have the lowest depth, so we're forced to sample the background behind transparent object
            }

            backLayer = ppll[frontLayer.nextFragmentIndex]; 
            layerCount += 2;
        } while(frontLayer.depth < lastDepth && backLayer.nextFragmentIndex != NO_TRANSPARENCY_INDEX && layerCount < MAX_TRANSPARENCY_LAYERS);

        // NOTE: I don't think we need this, unless we encounter some whacky geometry
        if (frontLayer.depth < lastDepth)
        {
            break;
        }
        backLayer = ppll[frontLayer.nextFragmentIndex]; 


        lastDepth = backLayer.depth;
        rayEndPos = backLayer.pos;

        vec4 transparencyVolumeColor = under(backLayer.color, frontLayer.color);
        // Effectively only the front surface reflects light
        transparencyVolumeColor.rgb = shade(frontLayer.pos, transparencyVolumeColor.rgb, frontLayer.normal, 256.f, 5.f, uv.xy);

        color = under(transparencyVolumeColor, color);

        uv = viewProjection * vec4(rayEndPos, 1.f);
        uv /= uv.w; 
        uv = uv * 0.5 + 0.5;
    }

    if (layerCount == 0)
    {
        return NO_TRANSPARENCY_COLOR;
    }

    return under(vec4(shadeFromTex(uv.xy), 1.f), color);
}

void main()
{
    vec2 fragmentPos = gl_FragCoord.xy / vec2(viewportWidth, viewportHeight);

    vec4 color = traceTransparency();
    if (color == NO_TRANSPARENCY_COLOR)
    {
        color = vec4(shadeFromTex(fragmentPos), 1.f);
    }

    fragColor = gammaCorrect(color.xyz, gamma);
}
