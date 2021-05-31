#version 330 core
layout (location = 0) out vec3 gPosition;
layout (location = 1) out vec3 gNormal;
layout (location = 2) out vec4 gAlbedo;
layout (location = 3) out vec3 gSpec;

in vec3 WorldFragPos;
in vec4 FragPos;
in vec2 TexCoords;
in vec3 Normal;
in mat3 Tbn;
in vec3 Tangent;

uniform bool usingNormalMap;

uniform bool showModelNormals;
uniform bool showNonTBNNormals;

uniform sampler2D tex_diffuse;
uniform sampler2D tex_normal;
//uniform sampler2D tex_specular;

void main()
{    
    gPosition = WorldFragPos.xyz;

    vec3 normal;
    if (usingNormalMap && !showModelNormals)
    {
        normal = texture(tex_normal, TexCoords).rgb;
        if (!showNonTBNNormals)
        {
            normal = normal * 2.f - 1.f;
            normal = Tbn * normal;
        }
    }
    else
    {
        //normal = Tangent;
        normal = Normal;
    }
    gNormal = normalize(normal);

    gAlbedo.rgb = texture(tex_diffuse, TexCoords).rgb;

    gSpec.rgb = FragPos.zzz / 10000;
    /*
    if (usingSpecularMap)
    {
        gSpec.rgb = texture(tex_specular, TexCoords).rrr;
    }
    */
}

