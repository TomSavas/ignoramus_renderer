#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec4 aTangent;
layout (location = 3) in vec2 aTexCoords;

out vec3 vWorldFragPos;
out vec4 vFragPos;
out vec2 vTexCoords;
out vec3 vNormal;
out mat3 vTbn;
out vec3 vTangent;

uniform bool usingNormalMap;

uniform bool showModelNormals;
uniform bool showNonTBNNormals;

layout (std140) uniform CameraParams
{
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
    vec4 cameraPos;
    vec4 nearFarPlanes;
};

layout (std140) uniform ModelParams
{
    mat4 model;
};

void main()
{
    vWorldFragPos = (model * vec4(aPos, 1.0)).xyz;
    vFragPos = projection * view * model * vec4(aPos, 1.0);
    vTexCoords = aTexCoords;
                    
    mat3 normalRecalculationMatrix = transpose(inverse(mat3(model)));
    vNormal = normalize(normalRecalculationMatrix * aNormal.xyz);
    if (usingNormalMap && !showModelNormals)
    {
        vec3 N = vNormal;
        vec3 T = normalize(normalRecalculationMatrix * aTangent.xyz);
        T = normalize(T - dot(T, N) * N);
        vec3 B = cross(N, T) * aTangent.w;

        vTangent = T;
        vTbn = mat3(T, B, N);
    }

    gl_Position = vFragPos;
}
