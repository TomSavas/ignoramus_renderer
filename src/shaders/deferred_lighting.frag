#version 330
layout (location = 0) out vec4 fragColor;

uniform sampler2D tex_pos;
uniform sampler2D tex_diffuse;
uniform sampler2D tex_normal;
uniform sampler2D tex_specular;

uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 cameraPos;

void main()
{
    vec2 fragmentPos = gl_FragCoord.xy / vec2(1920.f, 1080.f);

    vec4 pos = texture(tex_pos, fragmentPos);
    vec4 diffuse = texture(tex_diffuse, fragmentPos);
    vec4 normal = texture(tex_normal, fragmentPos);
    vec4 specular = texture(tex_specular, fragmentPos);

    vec3 toLight = lightPos - pos.xyz; 

    vec4 ambientIntensityColor = diffuse * 0.1f;
    vec4 diffuseIntensityColor = diffuse * max(dot(normal.xyz,normalize(toLight)), 0.f);
    float specularIntensity = pow(max(dot(reflect(normalize(pos.xyz - lightPos), normalize(normal.xyz)), normalize(cameraPos)), 0.f), 16.f) * specular.x;
    vec4 specularIntensityColor = diffuse * vec4(specularIntensity);

    fragColor = ambientIntensityColor + diffuseIntensityColor + specularIntensityColor;

    // lighting calculations
}
