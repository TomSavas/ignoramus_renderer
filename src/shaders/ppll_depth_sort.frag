#version 430 core
in vec3 WorldFragPos;
in vec4 FragPos;
in vec2 TexCoords;
in vec3 Normal;
in mat3 Tbn;
in vec3 Tangent;

in vec3 Barycentric;

out vec4 FragColor;

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
#define MAX_TRANSPARENCY_LAYERS 32
    TransparencyData layers[MAX_TRANSPARENCY_LAYERS];
    uint head = imageLoad(ppllHeads, ivec2(gl_FragCoord.xy)).r;

    int layerCount = 0;
    uint index = head;
    uint indices[MAX_TRANSPARENCY_LAYERS];
    while(index != 0xffffffff && layerCount < MAX_TRANSPARENCY_LAYERS)
    {
        layers[layerCount] = ppll[index];
        indices[layerCount] = index;
        index = layers[layerCount++].nextFragmentIndex;
    }

    // Sort back-to-front
    TransparencyData temporarySwapper;
    for (int i = 0; i < layerCount; i++)
    {
        for (int j = i; j < layerCount; j++)
        {
            if (layers[i].depth > layers[j].depth)
            {
                temporarySwapper = layers[j];
                layers[j] = layers[i];
                layers[i] = temporarySwapper;
            }
        }
    }

    // Write back
    for (int i = 0; i < layerCount-1; i++)
    {
        layers[i].nextFragmentIndex = indices[i+1];
    }
    layers[layerCount-1].nextFragmentIndex = 0xffffffff;
    for (int i = 0; i < layerCount; i++)
    {
        ppll[indices[i]] = layers[i];
    }
}
