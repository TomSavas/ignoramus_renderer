#version 330 core

vec3 calculateDiffuse(vec3 color, vec3 normal, vec3 lightDir)
{
    return color * max(0.f, dot(normal, -lightDir));
}

vec3 calculateSpecular(vec3 color, vec3 pos, vec3 normal, vec3 cameraPos, vec3 lightDir, float specularity, float specularPower)
{
    vec3 cameraToFrag = normalize(pos - cameraPos.xyz);
    float specularIntensity = pow(max(dot(cameraToFrag, reflect(-lightDir, normal)), 0.f), 1 / specularity * specularPower);

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
