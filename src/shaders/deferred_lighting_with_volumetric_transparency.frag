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
        //} while(false);

        // NOTE: I don't think we need this, unless we encounter some whacky geometry
        if (frontLayer.depth < lastDepth)
        {
            break;
            //return NO_TRANSPARENCY_COLOR;
        }
        backLayer = ppll[frontLayer.nextFragmentIndex]; 

        // TODO: move to transparency material
        const float refractiveIndex = 1.52f;
        //const float refractiveIndex = 1.f;
        //thickness = linearizeDepthFromCameraParams(frontLayer.depth) - linearizeDepthFromCameraParams(backLayer.depth);
        thickness = linearizeDepthFromCameraParams(backLayer.depth) - linearizeDepthFromCameraParams(frontLayer.depth);
        thickness /= nearFarPlanes.y;
        thickness *= pow(thickness, 3.f);//thickness * thickness * thickness;
        thickness *= nearFarPlanes.y;
        thickness *= 90000;
        thickness *= 90000;
        //thickness /= 100;
        vec3 toFragment = normalize(frontLayer.pos - cameraPos.xyz);
        vec3 refractedDir = refract(toFragment, frontLayer.normal, 1.f / refractiveIndex);
        // NOTE: not quite correct -- we're not un-refracting the ray
        rayEndPos = frontLayer.pos + refractedDir * thickness;
        lastDepth = backLayer.depth;

        float t = linearizeDepthFromCameraParams(backLayer.depth) - linearizeDepthFromCameraParams(frontLayer.depth);
        t /= nearFarPlanes.y;
        t *= t * t;
        t *= nearFarPlanes.y;
        t *= 9000;
        t = 1.f + t/8.f;


        float frontLayerTransparency = min(1.f, frontLayer.color.a * t);
        vec4 frontLayerColor = vec4(shade(frontLayer.pos, frontLayer.color.rgb, frontLayer.normal, 0.f, 1.f, uv.xy), frontLayerTransparency);

        float backLayerTransparency = backLayer.color.a;
        vec4 backLayerColor = vec4(shade(backLayer.pos, backLayer.color.rgb, backLayer.normal, 0.f, 1.f, uv.xy), backLayerTransparency);

        //vec4 transparencyVolumeColor = under(transparencyLayerColor(backLayer), transparencyLayerColor(frontLayer));
        //vec4 transparencyVolumeColor = under(backLayerColor, frontLayerColor);

        vec4 transparencyVolumeColor = under(backLayer.color, frontLayer.color);
        transparencyVolumeColor.rgb = shade(frontLayer.pos, transparencyVolumeColor.rgb, frontLayer.normal, 256.f, 5.f, uv.xy);

        color = under(transparencyVolumeColor, color);
        //color = vec4(vec3(thickness), 1.f);
        //color = frontLayerColor;

        uv = viewProjection * vec4(rayEndPos, 1.f);
        uv /= uv.w; 
        uv = uv * 0.5 + 0.5;
    }

    if (layerCount == 0)
    {
        return NO_TRANSPARENCY_COLOR;
    }

    return under(vec4(shadeFromTex(uv.xy), 1.f), color);
    //return color;
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
