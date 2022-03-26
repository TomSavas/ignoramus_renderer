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
layout (binding = transparencyDepth_AUTO_BINDING, r32ui) uniform uimage2D transparencyDepth;

float linearizeDepthFromCameraParams(float depth);
vec3 gammaCorrect(vec3 color, float gamma);

uniform sampler2D accumulator;
uniform sampler2D revealage;

const float EPSILON = 0.00001f;

bool isApproximatelyEqual(float a, float b)
{
    return abs(a - b) <= (abs(a) < abs(b) ? abs(b) : abs(a)) * EPSILON;
}

float max3(vec3 v) 
{
    return max(max(v.x, v.y), v.z);
}

vec4 weightedBlended(vec2 uv)
{
    float revealage = texture(revealage, uv).r;

    // save the blending and color texture fetch cost if there is not a transparent fragment
    if (isApproximatelyEqual(revealage, 1.0f)) 
        return vec4(0.f);

    vec4 accumulation = texture(accumulator, uv);

    // suppress overflow
    if (isinf(max3(abs(accumulation.rgb)))) 
        accumulation.rgb = vec3(accumulation.a);

    // prevent floating point precision bug
    vec3 average_color = accumulation.rgb / max(accumulation.a, EPSILON);

    return vec4(average_color, 1.0f - revealage);
}

vec4 under(vec4 backColor, vec4 frontColor)
{
    vec4 blendedColor;
    blendedColor.rgb = frontColor.rgb + backColor.rgb * backColor.a * (1.f - frontColor.a);
    blendedColor.a = frontColor.a + backColor.a * (1.f - frontColor.a);

    return blendedColor;
}

vec4 over(vec4 frontColor, vec4 backColor)
{
    vec4 blendedColor;
    blendedColor = backColor * (1.f - backColor.a) + frontColor * frontColor.a;

    return blendedColor;
}


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

    vec4 startingUv = vec4(gl_FragCoord.xy / vec2(1920.f, 1080.f), 0.f, 0.f);
    vec4 uv = startingUv;
    bool inFrontOfAllTranspGeometry = false;

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


        //float frontLayerTransparency = min(1.f, frontLayer.color.a * t);
        //vec4 frontLayerColor = vec4(shade(frontLayer.pos, frontLayer.color.rgb, frontLayer.normal, 0.f, 1.f, uv.xy), frontLayerTransparency);
        //float backLayerTransparency = backLayer.color.a;
        //vec4 backLayerColor = vec4(shade(backLayer.pos, backLayer.color.rgb, backLayer.normal, 0.f, 1.f, uv.xy), backLayerTransparency);

        //vec4 transparencyVolumeColor = under(transparencyLayerColor(backLayer), transparencyLayerColor(frontLayer));
        //vec4 transparencyVolumeColor = under(backLayerColor, frontLayerColor);

        vec4 newUv = viewProjection * vec4(rayEndPos, 1.f);
        newUv /= newUv.w; 
        newUv = newUv * 0.5 + 0.5;

        vec4 transparencyVolumeColor = under(backLayer.color, frontLayer.color);
        {
            float weightedBlendedDepth = float(imageLoad(transparencyDepth, ivec2(newUv.xy * vec2(viewportWidth, viewportHeight))).r) / nearFarPlanes.y;

            float nextDepth = 1.f;

            uint h = imageLoad(ppllHeads, ivec2(newUv.xy * vec2(1920.f, 1080.f))).r;

            for (int i = 0; i < 32 && h != NO_TRANSPARENCY_INDEX; i++)
            {
                float depth = ppll[h].depth;
                if(backLayer.depth < depth)
                {
                    nextDepth = min(nextDepth, depth);
                }

                // Ignore back layer
                h = ppll[h].nextFragmentIndex;
                if (h != NO_TRANSPARENCY_INDEX)
                {
                    h = ppll[h].nextFragmentIndex;
                }
            }

            bool inFront = weightedBlendedDepth < frontLayer.depth;
            bool behind = weightedBlendedDepth > backLayer.depth;
            bool inBetween = !inFront && behind && weightedBlendedDepth < nextDepth;

            if (inFront)
            {
                inFrontOfAllTranspGeometry = true;
            } 
            if (inBetween)
            {
                vec4 weightedBlendedColor = weightedBlended(newUv.xy);
                transparencyVolumeColor = under(weightedBlendedColor, transparencyVolumeColor);
            }
        }

        transparencyVolumeColor.rgb = shade(frontLayer.pos, transparencyVolumeColor.rgb, frontLayer.normal, 256.f, 5.f, uv.xy);

        color = under(transparencyVolumeColor, color);
        uv = newUv;
    }

    if (inFrontOfAllTranspGeometry)
    {
        color = under(color, weightedBlended(startingUv.xy));
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
        vec4 weightedBlendedColor = weightedBlended(fragmentPos);
        color = vec4(shadeFromTex(fragmentPos), 1.f);
        if (weightedBlendedColor.a > 0.f)
        {
            // I have seriously messed something up with under/over operators. And god only knows
            // why this is needed or why it works...
            color.a = weightedBlendedColor.a;
            color = over(weightedBlendedColor, color);
        }
    }

    fragColor = gammaCorrect(color.xyz, gamma);
}
