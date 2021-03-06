#version 460 

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

struct LightTile 
{
    uint count;
    uint offset;
};

layout (binding = lightIdCount_AUTO_BINDING) uniform atomic_uint lightIdCount;
layout (binding = LightTileData_AUTO_BINDING, std430) buffer LightTileData
{
    LightTile lightTiles[LIGHT_TILE_COUNT];
};

layout (binding = LightIds_AUTO_BINDING, std430) buffer LightIds
{
    uint lightIds[];
};

struct PointLight
{
    vec4 color;
    vec4 pos;
    vec4 radius;
};
layout (std430, binding=PointLights_AUTO_BINDING) buffer PointLights
{
    PointLight pointLights[MAX_POINT_LIGHTS];
    int pointLightCount;
};

uniform sampler2D tex_pos;
uniform sampler2D tex_normal;
uniform sampler2D tex_diffuse;
uniform sampler2D tex_specular;

uniform sampler2D shadow_map;

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
vec3 calculateSpecular(vec3 color, vec3 pos, vec3 normal, vec3 cameraPos, vec3 lightDir, float shininess);
vec3 composeColor(float ambientIntensity, float shadowIntensity, vec3 ambientColor, vec3 diffuse, vec3 specular);
vec3 gammaCorrect(vec3 color, float gamma);
vec3 heatmapGradient(float percentage);

const float ambientIntensity = 0.1;
vec3 shade(vec3 pos, vec3 color, vec3 normal, float specularity, float specularStrength, vec2 uv)
{
    int lightTileId = int(uv.x * LIGHT_TILE_COUNT_X) + int((1.f - uv.y) * LIGHT_TILE_COUNT_Y) * LIGHT_TILE_COUNT_X;

    float shadowIntensity = directionalShadowIntensity(pos, normal);
    vec3 diffuse = calculateDiffuse(color, normal, directionalLightDir.xyz) * (1.f - shadowIntensity);
    vec3 specular = calculateSpecular(vec3(1.f), pos, normal, cameraPos.xyz, directionalLightDir.xyz, specularity) * (1.f - shadowIntensity) * specularStrength;

    LightTile lightTile = lightTiles[lightTileId];

//#define TILE_HEATMAP_DEBUG
#ifdef TILE_HEATMAP_DEBUG
    float percentage = float(lightTile.count) / MAX_POINT_LIGHTS * 20.f;
    vec3 lightDensityHeat = heatmapGradient(percentage);
    return mix(calculateDiffuse(lightDensityHeat, normal, directionalLightDir.xyz), lightDensityHeat, 0.6);
#endif

    for (int i = 0; i < lightTile.count; i++)
    //for (int i = 0; i < 0; i++)
    {
        uint index = lightIds[i+lightTile.offset];
        vec3 lightDir = pointLights[index].pos.xyz - pos;
        float dist = length(lightDir);
        float r = pointLights[index].radius.x;

        float normalizedDist = (r - dist) / r; 
        float strength = pow(max(normalizedDist, 0.f), 2.f);

        diffuse += calculateDiffuse(color, normal, normalize(-lightDir)) * strength * pointLights[index].color.xyz;
        specular += calculateSpecular(pointLights[index].color.rgb, pos, normal, cameraPos.xyz, normalize(-lightDir), specularity) * strength * specularStrength;
    }

    return composeColor(ambientIntensity, 0.f, color, diffuse, specular);
}

vec3 shadeFromTex(vec2 uv)
{
    vec3 pos = texture(tex_pos, uv).xyz;
    vec3 color = texture(tex_diffuse, uv).rgb;
    vec3 normal = texture(tex_normal, uv).xyz;

    // TODO: no more specular map since we switched to PBR, handle this later
    //float specularity = texture(tex_specular, uv).r;

    return shade(pos, color, normal, 64.f, 0.1f, uv);
}
