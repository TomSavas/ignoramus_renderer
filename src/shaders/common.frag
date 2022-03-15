#version 330 core

vec3 calculateDiffuse(vec3 color, vec3 normal, vec3 lightDir)
{
    return color * max(0.f, dot(normal, -lightDir));
}

vec3 calculateSpecular(vec3 color, vec3 pos, vec3 normal, vec3 cameraPos, vec3 lightDir, float specularity)
{
    vec3 cameraToFrag = normalize(pos - cameraPos.xyz);
    float specularIntensity = pow(max(dot(cameraToFrag, reflect(-lightDir, normal)), 0.f), specularity);

    return color * specularIntensity;
}

vec3 composeColor(float ambientIntensity, float shadowIntensity, vec3 ambientColor, vec3 diffuse, vec3 specular)
{
    ambientColor = ambientColor * ambientIntensity;

    float lightIntensity = (1.f - shadowIntensity);
    vec3 litColor = lightIntensity * (diffuse + specular);

    return ambientColor + litColor * (1.0 - ambientIntensity);
}

vec3 gammaCorrect(vec3 color, float gamma)
{
    return pow(color, vec3(1.f / gamma));
}

vec3 heatmapGradient(float percentage)
{
#define GRADIENT_COLOR_COUNT 6
    const vec3 gradient[GRADIENT_COLOR_COUNT] = vec3[] 
        ( 
             vec3(0.f, 0.f, 0.1f),
             vec3(0.f, 0.f, 0.8f),
             vec3(0.f, 0.6f, 0.6f),
             vec3(0.f, 0.8f, 0.f),
             vec3(0.8f, 0.8f, 0.f),
             vec3(0.8f, 0.f, 0.f)
        );

    vec3 color = gradient[0];
    for (int i = 1; i < GRADIENT_COLOR_COUNT; i++)
    {
        const float multiple = GRADIENT_COLOR_COUNT - 1;
        color = mix(color, gradient[i], clamp((percentage - float(i - 1) * 1.f/multiple) * multiple, 0.f, 1.f));
    }

    return color;
}
