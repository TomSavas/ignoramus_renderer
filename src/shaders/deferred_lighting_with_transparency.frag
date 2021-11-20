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

    // Transparency
    {
#define MAX_TRANSPARENCY_LAYERS 32
        int layerCount = 0;
        TransparencyData layers[MAX_TRANSPARENCY_LAYERS];
        uint head = imageLoad(ppllHeads, ivec2(gl_FragCoord.xy)).r;

        // Gather transparency data
        uint index = head;
        while(index != 0xffffffff && layerCount < MAX_TRANSPARENCY_LAYERS)
        {
            layers[layerCount] = ppll[index];
            index = layers[layerCount++].nextFragmentIndex;
        }

        // Sort back-to-front
        TransparencyData temporarySwapper;
        for (int i = 0; i < layerCount; i++)
        {
            for (int j = i; j < layerCount; j++)
            {
                if (layers[j].depth > layers[i].depth)
                {
                    temporarySwapper = layers[j];
                    layers[j] = layers[i];
                    layers[i] = temporarySwapper;
                }
            }
        }

        const float refractiveIndex = 1.52f;
        for (int i = 0; i < layerCount; i += 2)
        {
            TransparencyData backLayer = layers[i];
            TransparencyData frontLayer = layers[i+1];

            float thickness = linearizeDepthFromCameraParams(backLayer.depth) - linearizeDepthFromCameraParams(frontLayer.depth);
            thickness *= 2.f;
            vec3 toFragment = normalize(frontLayer.pos - cameraPos.xyz);
            vec3 refractedDir = refract(toFragment, frontLayer.normal, 1.f / refractiveIndex);

            vec4 newPos = vec4(refractedDir * thickness + backLayer.pos, 1.f);
            newPos = viewProjection * newPos;
            newPos /= newPos.w;
            newPos = newPos * 0.5 + 0.5;

            transparencyColor = texture(tex_diffuse, newPos.xy);
        }

        // Compose
        for (int i = 0; i < layerCount; i++)
        {
            float alpha = 1.f - transparencyColor.a;

            vec3 transpDiffuse = calculateDiffuse(layers[i].color.rgb, layers[i].normal, directionalLightDir.xyz);
            vec3 transpSpecular = calculateSpecular(layers[i].color.rgb, layers[i].pos, layers[i].normal, cameraPos.xyz, directionalLightDir.xyz, 10.f, specularPower);
            vec3 transpColor = composeColor(0.1f, 0.f, layers[i].color.rgb, transpDiffuse, transpSpecular);

            transparencyColor.rgb = transpColor * layers[i].color.a + transparencyColor.rgb * (1.f - layers[i].color.a);
            transparencyColor.a = transparencyColor.a + layers[i].color.a * alpha;
        }
    }

    fragColor = gammaCorrect(transparencyColor.xyz, gamma);
}
