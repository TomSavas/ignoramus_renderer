#version 330
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

float linearizeDepth(float d, float zNear, float zFar)
{
    d = 2.f * d - 1.f;
    return 2.f * zNear * zFar / (zFar + zNear - d * (zFar - zNear));
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
    float currentDepth = lightSpaceFragPos.z / farPlane;

    float bias = max(directionalAngleBias * (1.0 - dot(normalize(normal), normalize(lightToFrag))), directionalBias) * farPlane;

    // PCF
    vec2 texelSize = 1.f / textureSize(shadow_map, 0);
    float pcfShadow = 0;
    for (int x = -1; x <= 1; x++)
    {
        for (int y = -1; y <= 1; y++)
        {
            float pcfDepth = linearizeDepth(texture(shadow_map, normalizedLightSpaceFragPos.xy + vec2(x, y) * texelSize).r, nearPlane, farPlane) / farPlane;
            pcfShadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0; 
        }
    }
    shadow += pcfShadow / 9.f;

    return shadow;
}

const float ambientIntensity = 0.1;
void main()
{
    vec2 fragmentPos = gl_FragCoord.xy / vec2(viewportWidth, viewportHeight);

    vec3 pos = texture(tex_pos, fragmentPos).xyz;
    vec3 diffuse = texture(tex_diffuse, fragmentPos).rgb;
    vec3 normal = texture(tex_normal, fragmentPos).xyz;
    float specular = texture(tex_specular, fragmentPos).r;

    vec3 ambientIntensityColor = diffuse * ambientIntensity;
    vec3 diffuseIntensityColor = diffuse * max(0.f, dot(normal.xyz, -directionalLightDir.xyz));

    vec3 cameraToFrag = normalize(pos - cameraPos.xyz);
    float specularIntensity = pow(max(dot(cameraToFrag, reflect(-directionalLightDir.xyz, normal)), 0.f), specular * specularPower);
    vec3 specularIntensityColor = diffuse * specularIntensity;

    float shadowIntensity = directionalShadowIntensity(pos, normal);
    float lightIntensity = (1.f - shadowIntensity);
    vec3 litColor = lightIntensity * (diffuseIntensityColor + specularIntensityColor);

    vec3 color = ambientIntensityColor + litColor * (1.0 - ambientIntensity);

    // Gamma correction
    fragColor = pow(color, vec3(1.f / gamma));
}
