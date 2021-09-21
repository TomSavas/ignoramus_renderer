#version 330
//layout (location = 0) out vec4 fragColor;
out vec4 fragColor;

layout (std140) uniform SceneParams
{
    int pixelSize;
    bool wireframe;
    float gamma;
};

layout (std140) uniform CameraParams
{
    mat4 view;
    mat4 projection;
    vec3 cameraPos;
};

layout (std140) uniform Lighting
{
    bool usingShadowCubemap;
    bool usingShadowMap;

    vec3 pointLightPos;
    vec3 directionalLightPos;

    mat4 directionalLightViewProjection;

    float directionalBias;
    float directionalAngleBias;

    float pointBias;
    float pointAngleBias;
};

uniform samplerCube shadow_cubemap;
uniform sampler2D shadow_map;

uniform sampler2D tex_pos;
uniform sampler2D tex_diffuse;
uniform sampler2D tex_normal;
uniform sampler2D tex_specular;

//uniform vec3 pointLightPos;
uniform vec3 lightColor;

//uniform vec3 directionalLightPos;
//uniform mat4 directionalLightViewProjection;

//uniform bool usingShadowCubemap;
//uniform bool usingShadowMap;

float farPlane = 100000.f;

//uniform float directionalBias;
//uniform float directionalAngleBias;
//
//uniform float pointBias;
//uniform float pointAngleBias;


float linearize_depth(float d, float zNear, float zFar)
{
    d = 2.f * d - 1.f;
    return 2.f * zNear * zFar / (zFar + zNear - d * (zFar - zNear));
}

float shadowIntensity(vec4 fragPos, vec3 normal)
{
    // Something very very VERY bad is going on... usingShadowCubemap seems to be set to true
    // there has to be some major uniform fuck up happening then
    //return 0.f;


    if (!usingShadowCubemap && !usingShadowMap)
        return 0.f;

    float shadow = 0.f;

    //TEMP: disable due to not having a dummy cubemap to bind
    /*
    // point lights
    if (usingShadowCubemap)
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
            // TODO: just changed this on a whim, but I don't think it should be shadow_map
            //vec2 texelSize = 1.f / textureSize(shadow_map, 0);
            vec2 texelSize = 1.f / textureSize(shadow_cubemap, 0);

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
                        pcfShadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0; 
                    }
                }
            }
            shadow += pcfShadow / (samples * samples * samples);
        }
    }
    */

    // directional lights
    if (usingShadowMap)
    {
        vec4 lightSpaceFragPos = directionalLightViewProjection * vec4(fragPos.xyz, 1.f);
        vec3 lightSpaceFragPosNDC = lightSpaceFragPos.xyz / lightSpaceFragPos.w;
        vec3 normalizedLightSpaceFragPos = lightSpaceFragPosNDC * 0.5f + 0.5f;

        //float currentDepth = normalizedLightSpaceFragPos.z;// * 100000.f;

        vec3 lightToFrag = fragPos.xyz - directionalLightPos;
        float currentDepth = lightSpaceFragPos.z / farPlane * 8.f; // <-------- TODO: figure out what the fuck is up with this retarded shit -- why the far planes differ for camera/light projections?
        //float currentDepth = length(lightToFrag);

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
                //float pcfDepth = texture(shadow_map, normalizedLightSpaceFragPos.xy + vec2(x, y) * texelSize).r * 100000.f;
                float pcfDepth = linearize_depth(texture(shadow_map, normalizedLightSpaceFragPos.xy + vec2(x, y) * texelSize).r, 1.f, 100000.f) / farPlane;
                //float depth = linearize_depth(texture(shadow_map, fragmentPos).r, 1.f, 100000.f) / 100000.f;
                pcfShadow += currentDepth > pcfDepth ? 1.0 : 0.0; 
            }
        }
        shadow += pcfShadow / 9.f;
    }

    return shadow / ((usingShadowMap && usingShadowCubemap) ? 2.f : 1.f);
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
    vec4 color = ambientIntensityColor + (1.f - shadowIntensity(pos, normal.xyz)) * (diffuseIntensityColor + specularIntensityColor);

    // Gamma correction
    fragColor = pow(color, vec4(vec3(1.f / gamma), 1.f));
    //fragColor = specularIntensityColor;

    //float depth = linearize_depth(texture(shadow_map, fragmentPos).r, 1.f, 100000.f) / farPlane;
    //vec4 lightSpaceFragPos = directionalLightViewProjection * vec4(pos.xyz, 1.f);
    //float depth = lightSpaceFragPos.z / farPlane * 10.f;
    //fragColor = vec4(vec3(depth), 1.f);
    //fragColor = vec4(vec3(currentDepth), 1.f);
}
