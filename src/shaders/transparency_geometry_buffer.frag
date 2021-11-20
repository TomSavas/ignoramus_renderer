#version 430 core
//layout (location = 0) out vec3 gPosition;
//layout (location = 1) out vec3 gNormal;
//layout (location = 2) out vec3 gAlbedo;
//layout (location = 3) out vec3 gSpec;

//out vec4 FragColor;

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
    float gamma;
    float specularPower;
    float viewportWidth;
    float viewportHeight;
};

struct TransparencyData
{
    vec4 color;
    vec3 pos;
    vec3 normal;
    float depth;
    uint nextFragmentIndex;
};

layout (std140) uniform MaterialParams
{
    vec4 tintAndOpacity;
};

layout (binding = 0, r32ui) uniform uimage2D ppllHeads;
layout (binding = 1) uniform atomic_uint transparentFragmentCount;
layout (binding = 2, std430) buffer TransparentFragments
{
    TransparencyData ppll[];
};

void main()
{    
    const int maxTransparencyLayers = 4;
    const int linkedListSize = 1920 * 1080 * maxTransparencyLayers;
    if (atomicCounter(transparentFragmentCount) >= linkedListSize)
    {
        // TODO: try to replace the existing data with data closer to the camera
        return;
    }

    uint index = atomicCounterIncrement(transparentFragmentCount);
    uint lastHead = imageAtomicExchange(ppllHeads, ivec2(gl_FragCoord.xy), index);

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
    
    ppll[index].normal = normalize(normal);

    ppll[index].color = tintAndOpacity;
    ppll[index].pos = WorldFragPos;
    ppll[index].depth = gl_FragCoord.z;
    ppll[index].nextFragmentIndex = lastHead;
}