#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 aTangent;
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

//uniform mat4 model;
layout (std140) uniform ModelParams
{
    mat4 model;
};

void main()
{
    vWorldFragPos = (model * vec4(aPos, 1.0)).xyz;
    vFragPos = projection * view * model * vec4(aPos, 1.0);
    vTexCoords = aTexCoords;
                    
    if (usingNormalMap && !showModelNormals)
    {
        vec3 N = normalize(vec3(model * vec4(aNormal, 0.f)));
        vec3 tan = normalize(aTangent);
        vec3 T = normalize(vec3(model * vec4(tan, 0.f)));
        vTangent = T;
        vec3 B = cross(N, T);
        vTbn = mat3(T, B, N);
    }
    else
    {
        mat3 normalRecalculationMatrix = transpose(inverse(mat3(model)));
        vNormal = normalRecalculationMatrix * aNormal;
    }

    vec3 tan = normalize(aTangent);
    vec3 T = normalize(vec3(model * vec4(tan, 0.f)));
    vTangent = T;

    mat3 m = transpose(inverse(mat3(model)));

    //gl_Position = projection * view * worldPos;
    gl_Position = vFragPos;
}
