#version 330
layout (location = 0) out vec4 fragColor;

uniform sampler2D tex_pos;
uniform sampler2D tex_diffuse;
uniform sampler2D tex_normal;
uniform sampler2D tex_specular;

uniform samplerCube shadow_cubemap;
uniform sampler2D shadow_map;

uniform vec3 pointLightPos;
uniform vec3 lightColor;

uniform vec3 cameraPos;

uniform vec3 directionalLightPos;
uniform mat4 directionalLightViewProjection;

uniform bool using_shadow_cubemap;
uniform bool using_shadow_map;

float farPlane = 100000.f;

uniform float directionalBias;
uniform float directionalAngleBias;

uniform float pointBias;
uniform float pointAngleBias;

float shadowIntensity(vec4 fragPos, vec3 normal)
{
    if (!using_shadow_cubemap && !using_shadow_map)
        return 0.f;

    float shadow = 0.f;

    // point lights
    if (using_shadow_cubemap)
    {
        vec3 lightToFrag = fragPos.xyz - pointLightPos;
        float lightNormalDot = dot(normalize(lightToFrag), normalize(normal));
        if (lightNormalDot > 0.f)
        {
            return 0.4f;
        }
        else
        {
            float currentDepth = length(lightToFrag);
            float bias = max(pointAngleBias * (1.0 - lightNormalDot), pointBias) * farPlane;  

            // Non PCF
            //float closestDepth = texture(shadow_cubemap, lightToFrag).r * farPlane;
            //shadow += currentDepth - bias > closestDepth ? 1.0 : 0.0; 

            // PCF
            vec2 texelSize = 1.f / textureSize(shadow_map, 0);

            float samples = 4.f;
            float offset = 1.0f;

            float pcfShadow = 0.f;
            for (float x = -offset; x < offset; x += offset / (samples / 2.f))
            {
                for (float y = -offset; y < offset; y += offset / (samples / 2.f))
                {
                    for (float z = -offset; z < offset; z += offset / (samples / 2.f))
                    {
                        float pcfDepth = texture(shadow_cubemap, lightToFrag + vec3(x, y, z)).r * 100000.f;
                        //float pcfDepth = texture(shadow_cubemap, lightToFrag).r * 100000.f;
                        pcfShadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0; 
                    }
                }
            }
            shadow += pcfShadow / (samples * samples * samples);
        }
    }

    // directional lights
    if (using_shadow_map)
    {
        vec4 lightSpaceFragPos = directionalLightViewProjection * vec4(fragPos.xyz, 1.f);
        vec3 lightSpaceFragPosNDC = lightSpaceFragPos.xyz / lightSpaceFragPos.w;
        vec3 normalizedLightSpaceFragPos = lightSpaceFragPosNDC * 0.5f + 0.5f;

        //float currentDepth = normalizedLightSpaceFragPos.z;// * 100000.f;

        vec3 lightToFrag = fragPos.xyz - directionalLightPos;
        float currentDepth = length(lightToFrag);

        float bias = max(directionalAngleBias * (1.0 - dot(normalize(normal), normalize(lightToFrag))), directionalBias) * farPlane;
        // Non PCF
        //float closestDepth = texture(shadow_map, normalizedLightSpaceFragPos.xy).r * 100000.f;// farPlane;
        //shadow += currentDepth - bias > closestDepth ? 1.0 : 0.0; 

        // PCF
        vec2 texelSize = 1.f / textureSize(shadow_map, 0);
        float pcfShadow = 0;
        for (int x = -1; x <= 1; x++)
        {
            for (int y = -1; y <= 1; y++)
            {
                float pcfDepth = texture(shadow_map, normalizedLightSpaceFragPos.xy + vec2(x, y) * texelSize).r * 100000.f;
                pcfShadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0; 
            }
        }
        shadow += pcfShadow / 9.f;
    }

    return shadow * 0.4;
}

float lightAttenuation(vec3 fragPos)
{
    vec3 lightToFrag = fragPos - pointLightPos;
    float sqrDistFromLight = dot(lightToFrag, lightToFrag);

    return min(1.0, 100000.f / sqrDistFromLight);
}

void main()
{
    vec2 fragmentPos = gl_FragCoord.xy / vec2(1920.f, 1080.f);

    vec4 pos = texture(tex_pos, fragmentPos);
    vec4 diffuse = texture(tex_diffuse, fragmentPos);
    vec4 normal = texture(tex_normal, fragmentPos);
    vec4 specular = texture(tex_specular, fragmentPos);

    vec3 toDirLight = directionalLightPos - pos.xyz; 
    vec3 toPointLight = pointLightPos - pos.xyz; 

    //float pointLightAttenuation = lightAttenuation(pos.xyz);

    vec4 ambientIntensityColor = diffuse * 0.1f;
    vec4 diffuseIntensityColor = diffuse * min((max(dot(normal.xyz,normalize(toDirLight)), 0.f) + max(dot(normal.xyz,normalize(toPointLight)), 0.f) * lightAttenuation(pos.xyz)), 1.f);
    float specularIntensity = pow(max(dot(reflect(normalize(pos.xyz - directionalLightPos), normalize(normal.xyz)), normalize(cameraPos)), 0.f), 16.f) * specular.x;
    vec4 specularIntensityColor = diffuse * vec4(specularIntensity);

    // TODO: fix shadow/light interactions
    fragColor = ambientIntensityColor + (1.f - shadowIntensity(pos, normal.xyz)) * (diffuseIntensityColor + specularIntensityColor);

    // TODO: add gamma correction
}
