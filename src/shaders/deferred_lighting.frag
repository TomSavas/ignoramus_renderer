#version 460
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

uniform sampler2D shadow_map;

uniform sampler2D tex_pos;
uniform sampler2D tex_diffuse;
uniform sampler2D tex_normal;
uniform sampler2D tex_specular;

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

float linearizeDepth(float depth, float near, float far)
{
    depth = 2.f * depth - 1.f;
    return 2.f * near * far / (far + near - depth * (far - near));
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
    //vec3 diffuse = diffuseColor(color, normal);
    vec3 specular = calculateSpecular(color, pos, normal, cameraPos.xyz, directionalLightDir.xyz, specularity, specularPower);
    float shadowIntensity = directionalShadowIntensity(pos, normal);

    vec3 uncorrectedColor = composeColor(ambientIntensity, shadowIntensity, color, diffuse, specular);
    fragColor = gammaCorrect(uncorrectedColor, gamma);
}
