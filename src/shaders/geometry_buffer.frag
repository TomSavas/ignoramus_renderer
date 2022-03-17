#version 330 core
layout (location = 0) out vec3 gPosition;
layout (location = 1) out vec3 gNormal;
layout (location = 2) out vec3 gAlbedo;
layout (location = 3) out vec3 gSpec;

in vec3 WorldFragPos;
in vec4 FragPos;
in vec2 TexCoords;
in vec3 Normal;
in mat3 Tbn;
in vec3 Tangent;

in vec3 Barycentric;

uniform bool usingNormalMap;

uniform bool showModelNormals;
uniform bool showNonTBNNormals;

uniform sampler2D tex_diffuse;
uniform sampler2D tex_normal;
uniform sampler2D tex_specular;

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

vec3 fwidth(vec3 vec)
{
      return abs(dFdx(vec)) + abs(dFdy(vec));
}

void main()
{    
    float minBary = 1;
    if (wireframe)
    {
        vec3 baryDeltas = fwidth(Barycentric);
        vec3 bary = smoothstep(0.5 * baryDeltas, baryDeltas, Barycentric);
        minBary = min(bary.x, min(bary.y, bary.z));
    }
    //minBary = max(wireframe, minBary); // disable wireframe

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
        normal = Normal;
    }

    gNormal = normalize(normal);
    gAlbedo = minBary * texture(tex_diffuse, TexCoords).rgb;

    //gSpec.rgb = texture(tex_specular, TexCoords).rrr;
    gSpec.rgb = vec3(1.f, 0.f, 0.f);
}

