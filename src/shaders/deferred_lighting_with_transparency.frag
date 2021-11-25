#version 430
layout (location = 0) out vec3 fragColor;

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

struct TransparencyData
{
    vec4 color;
    vec3 pos;
    vec3 normal;
    float depth;
    uint nextFragmentIndex;
};

layout (binding = 0, r32ui) uniform uimage2D ppllHeads;
layout (binding = 1) uniform atomic_uint transparentFragmentCount;
layout (binding = 2, std430) buffer TransparentFragments
{
    TransparencyData ppll[];
};

uniform sampler2D shadow_map;

uniform sampler2D tex_pos;
uniform sampler2D tex_diffuse;
uniform sampler2D tex_normal;
uniform sampler2D tex_specular;

float linearizeDepth(float depth, float near, float far)
{
    depth = 2.f * depth - 1.f;
    return 2.f * near * far / (far + near - depth * (far - near));
}

float linearizeDepthFromCameraParams(float depth)
{
    return linearizeDepth(depth, nearFarPlanes.x, nearFarPlanes.y);
}

float directionalShadowIntensity(vec3 pos, vec3 normal)
{
    float shadow = 0.f;

    float nearPlane = nearFarPlanes.x;
    float farPlane = nearFarPlanes.y;

    vec4 lightSpaceFragPos = directionalLightViewProjection * vec4(pos, 1.f);
    vec3 lightSpaceFragPosNDC = lightSpaceFragPos.xyz / lightSpaceFragPos.w;
    vec3 normalizedLightSpaceFragPos = lightSpaceFragPosNDC * 0.5f + 0.5f;

    float directionalBias = directionalBiasAndAngleBias.x;
    float directionalAngleBias = directionalBiasAndAngleBias.y;

    vec3 lightToFrag = directionalLightDir.xyz;
    float currentDepth = linearizeDepth(normalizedLightSpaceFragPos.z, nearPlane, farPlane);

    float bias = max(directionalAngleBias * (1.0 - dot(normalize(normal), normalize(lightToFrag))), directionalBias);

    // PCF
    vec2 texelSize = 1.f / textureSize(shadow_map, 0);
    float pcfShadow = 0;
    for (int x = -1; x <= 1; x++)
    {
        for (int y = -1; y <= 1; y++)
        {
            float pcfDepth = linearizeDepth(texture(shadow_map, normalizedLightSpaceFragPos.xy + vec2(x, y) * texelSize).r, nearPlane, farPlane);
            pcfShadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0; 
        }
    }
    shadow += pcfShadow / 9.f;

    return shadow;
}

vec3 calculateDiffuse(vec3 color, vec3 normal, vec3 lightDir);
vec3 calculateSpecular(vec3 color, vec3 pos, vec3 normal, vec3 cameraPos, vec3 lightDir, float specularity, float specularPower);
vec3 composeColor(float ambientIntensity, float shadowIntensity, vec3 ambientColor, vec3 diffuse, vec3 specular);
vec3 gammaCorrect(vec3 color, float gamma);

vec4 transparencyLayerColor(TransparencyData layer)
{
    vec3 transpDiffuse = calculateDiffuse(layer.color.rgb, layer.normal, directionalLightDir.xyz);
    vec3 transpSpecular = calculateSpecular(layer.color.rgb, layer.pos, layer.normal, cameraPos.xyz, directionalLightDir.xyz, 2.f, specularPower);
    vec3 transpColor = composeColor(0.1f, 0.f, layer.color.rgb, transpDiffuse, transpSpecular);

    return vec4(transpColor, layer.color.a);
}

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

vec4 traceTransparency()
{
#define MAX_TRANSPARENCY_LAYERS 16
#define NO_TRANSPARENCY_INDEX 0xffffffff
#define NO_TRANSPARENCY_COLOR vec4(-1.f)

    float lastDepth = 0;
    int layerCount = 0;
    vec4 color = vec4(0.f);
    vec3 rayEndPos;

    vec4 uv = vec4(gl_FragCoord.xy / vec2(1920.f, 1080.f), 0.f, 0.f);

    while (layerCount < MAX_TRANSPARENCY_LAYERS)
    {
        uint head = imageLoad(ppllHeads, ivec2(uv.xy * vec2(1920.f, 1080.f))).r;
        if (head == NO_TRANSPARENCY_INDEX)
        {
            break;
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

        // TODO: move to transparency material
        const float refractiveIndex = 1.52f;
        float thickness = linearizeDepthFromCameraParams(frontLayer.depth) - linearizeDepthFromCameraParams(backLayer.depth);
        vec3 toFragment = normalize(frontLayer.pos - cameraPos.xyz);
        vec3 refractedDir = refract(toFragment, frontLayer.normal, 1.f / refractiveIndex);
        // NOTE: not quite correct -- we're not un-refracting the ray
        rayEndPos = frontLayer.pos + refractedDir * thickness;
        lastDepth = backLayer.depth;

        vec4 transparencyVolumeColor = under(transparencyLayerColor(backLayer), transparencyLayerColor(frontLayer));
        color = under(transparencyVolumeColor, color);

        uv = viewProjection * vec4(rayEndPos, 1.f);
        uv /= uv.w; 
        uv = uv * 0.5 + 0.5;
    }

    if (layerCount == 0)
    {
        return NO_TRANSPARENCY_COLOR;
    }

    return under(vec4(texture(tex_diffuse, uv.xy).rgb, 1.f), color);
}

const float ambientIntensity = 0.1;
void main()
{
    vec2 fragmentPos = gl_FragCoord.xy / vec2(viewportWidth, viewportHeight);

    vec4 transparencyColor = traceTransparency();
    if (transparencyColor.x == NO_TRANSPARENCY_COLOR.x)
    {
        vec3 pos = texture(tex_pos, fragmentPos).xyz;
        vec3 color = texture(tex_diffuse, fragmentPos).rgb;
        vec3 normal = texture(tex_normal, fragmentPos).xyz;
        float specularity = texture(tex_specular, fragmentPos).r;

        vec3 diffuse = calculateDiffuse(color, normal, directionalLightDir.xyz);
        vec3 specular = calculateSpecular(color, pos, normal, cameraPos.xyz, directionalLightDir.xyz, specularity, specularPower);
        float shadowIntensity = directionalShadowIntensity(pos, normal);
        vec3 uncorrectedColor = composeColor(ambientIntensity, shadowIntensity, color, diffuse, specular);

        transparencyColor = vec4(uncorrectedColor, 1.f);
    }

    fragColor = gammaCorrect(transparencyColor.xyz, gamma);
}















/*
// Straight forward refraction only
const float ambientIntensity = 0.1;
void main()
{
    vec2 fragmentPos = gl_FragCoord.xy / vec2(viewportWidth, viewportHeight);

    vec3 pos = texture(tex_pos, fragmentPos).xyz;
    vec3 color = texture(tex_diffuse, fragmentPos).rgb;
    vec3 normal = texture(tex_normal, fragmentPos).xyz;
    float specularity = texture(tex_specular, fragmentPos).r;

    vec3 diffuse = calculateDiffuse(color, normal, directionalLightDir.xyz);
    vec3 specular = calculateSpecular(color, pos, normal, cameraPos.xyz, directionalLightDir.xyz, specularity, specularPower);
    float shadowIntensity = directionalShadowIntensity(pos, normal);
vec3 uncorrectedColor = composeColor(ambientIntensity, shadowIntensity, color, diffuse, specular);
    vec4 transparencyColor = vec4(uncorrectedColor, 0.0f);
    //vec4 transparencyColor = vec4(0.f);

    // Transparency
    {
#define MAX_TRANSPARENCY_LAYERS 32
        int layerCount = 0;
        TransparencyData layers[MAX_TRANSPARENCY_LAYERS];
        uint head = imageLoad(ppllHeads, ivec2(gl_FragCoord.xy)).r;

        // Gather transparency data
        uint index = head;
        while(index != 0xffffffff)// && layerCount < MAX_TRANSPARENCY_LAYERS)
        {
            layers[layerCount] = ppll[index];
            //layers[layerCount].color.a = 0.5f;
            index = layers[layerCount++].nextFragmentIndex;
        }

        const float refractiveIndex = 1.52f;
        vec3 samplePos = vec3(-1.f);
        if (layerCount > 1)
        {
            samplePos = layers[1].pos;
        }
        for (int i = 0; i < layerCount; i += 2)
        {
            TransparencyData backLayer = layers[i];
            TransparencyData frontLayer = layers[i+1];

            float thickness = linearizeDepthFromCameraParams(frontLayer.depth) - linearizeDepthFromCameraParams(backLayer.depth);
            thickness *= 0.2f;

            vec3 toFragment = normalize(frontLayer.pos - cameraPos.xyz);
            vec3 refractedDir = refract(toFragment, frontLayer.normal, 1.f / refractiveIndex);
            samplePos += refractedDir * thickness;

            //vec4 newPos = vec4(refractedDir * thickness + backLayer.pos, 1.f);
            //newPos = viewProjection * newPos;
            //newPos /= newPos.w;
            //newPos = newPos * 0.5 + 0.5;

            //transparencyColor = texture(tex_diffuse, newPos.xy);
            //transparencyColor = vec4(newPos.xy, 0.f, 1.f);
            //transparencyColor = vec4(vec3(thickness / 200.f), 1.f);
        }
        //transparencyColor = vec4(vec3(float(layerCount) / 16.f), 1.f);

        if (layerCount > 1)
        {
            transparencyColor = vec4(0.f, 0.f, 0.f, 0.f);

            // back-to-front
            //if (samplePos.x != -1.f)
            //{
            //    vec4 newPos = vec4(samplePos, 1.f);
            //    newPos = viewProjection * newPos;
            //    newPos /= newPos.w;
            //    newPos = newPos * 0.5 + 0.5;
            //    transparencyColor = texture(tex_diffuse, newPos.xy);
            //}

            // Compose
            for (int i = 0; i < layerCount; i += 2)
            {
                float alpha = 1.f - transparencyColor.a;

                vec3 transpDiffuse = calculateDiffuse(layers[i].color.rgb, layers[i].normal, directionalLightDir.xyz);
                vec3 transpSpecular = calculateSpecular(layers[i].color.rgb, layers[i].pos, layers[i].normal, cameraPos.xyz, directionalLightDir.xyz, 10.f, specularPower);
                vec3 transpColor = composeColor(0.1f, 0.f, layers[i].color.rgb, transpDiffuse, transpSpecular);

                // back-to-front
                //transparencyColor.rgb = transpColor * layers[i].color.a + transparencyColor.rgb * (1.f - layers[i].color.a);
                //transparencyColor.a = transparencyColor.a + layers[i].color.a * alpha;

                // front-to-back
                transparencyColor.rgb += transpColor * layers[i].color.a * alpha;
                transparencyColor.a += layers[i].color.a * alpha;
            }

            if (samplePos.x != -1.f)
            {
                vec4 newPos = vec4(samplePos, 1.f);
                newPos = viewProjection * newPos;
                newPos /= newPos.w;
                newPos = newPos * 0.5 + 0.5;
                //transparencyColor = texture(tex_diffuse, newPos.xy);
                vec3 c = texture(tex_diffuse, newPos.xy).rgb;

                float alpha = 1.f - transparencyColor.a;
                transparencyColor.rgb += c * 1.f * alpha;
                transparencyColor.a += 1.f * alpha;
            }
        }
        else
        {
            transparencyColor = vec4(uncorrectedColor, 0.0f);
        }
    }

    fragColor = gammaCorrect(transparencyColor.xyz, gamma);
}
*/
