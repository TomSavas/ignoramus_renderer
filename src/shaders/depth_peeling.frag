#version 330 core
in vec3 Pos;
in vec3 Normal;
//in vec2 uv;

out vec4 FragColor;

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

layout (std140) uniform MaterialParams
{
    vec4 tintAndOpacity;
};
uniform sampler2D greater_depth;

float visibleDepth(float depth)
{
    float near = 0.1f;
    float far = 100000.f;

    depth = (2.f * near) / (far + near - depth * (far - near));
    return depth * 20;
}

vec3 calculateDiffuse(vec3 color, vec3 normal, vec3 lightDir);
vec3 calculateSpecular(vec3 color, vec3 pos, vec3 normal, vec3 cameraPos, vec3 lightDir, float specularity, float specularPower);
vec3 composeColor(float ambientIntensity, float shadowIntensity, vec3 ambientColor, vec3 diffuse, vec3 specular);

void main()
{
    vec2 uv = gl_FragCoord.xy / vec2(1920.f, 1080.f);
    float z = gl_FragCoord.z;

    float minDepth = texture(greater_depth, uv).r;

    if (z <= minDepth)
    {
        discard;
    }

    vec3 diffuse = calculateDiffuse(tintAndOpacity.rgb, Normal, directionalLightDir.xyz);
    vec3 specular = calculateSpecular(tintAndOpacity.rgb, Pos, Normal, cameraPos.xyz, directionalLightDir.xyz, 10.f, specularPower);

    FragColor = vec4(composeColor(0.1f, 0.f, tintAndOpacity.rgb, diffuse, specular), tintAndOpacity.a);
}
