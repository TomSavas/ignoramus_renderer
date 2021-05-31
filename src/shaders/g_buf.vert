#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 aTangent;
layout (location = 3) in vec2 aTexCoords;

out vec3 WorldFragPos;
out vec4 FragPos;
out vec2 TexCoords;
out vec3 Normal;
out mat3 Tbn;
out vec3 Tangent;

uniform bool usingNormalMap;

uniform bool showModelNormals;
uniform bool showNonTBNNormals;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    WorldFragPos = (model * vec4(aPos, 1.0)).xyz;
    FragPos = projection * view * model * vec4(aPos, 1.0);
    TexCoords = aTexCoords;
                    
    if (usingNormalMap && !showModelNormals)
    {
        vec3 N = normalize(vec3(model * vec4(aNormal, 0.f)));
        vec3 tan = normalize(aTangent);
        vec3 T = normalize(vec3(model * vec4(tan, 0.f)));
        Tangent = T;
        vec3 B = cross(N, T);
        Tbn = mat3(T, B, N);
    }
    else
    {
        mat3 normalRecalculationMatrix = transpose(inverse(mat3(model)));
        Normal = normalRecalculationMatrix * aNormal;
    }

    vec3 tan = normalize(aTangent);
    vec3 T = normalize(vec3(model * vec4(tan, 0.f)));
    Tangent = T;

    mat3 m = transpose(inverse(mat3(model)));

    //gl_Position = projection * view * worldPos;
    gl_Position = FragPos;
}
