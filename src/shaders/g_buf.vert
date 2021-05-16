#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 aTangent;
//layout (location = 3) in vec3 aBitangent;
layout (location = 3) in vec2 aTexCoords;

out vec3 FragPos;
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
    vec4 worldPos = model * vec4(aPos, 1.0);
    FragPos = worldPos.xyz; 
    TexCoords = aTexCoords;
                    
    if (usingNormalMap && !showModelNormals)
    {
        vec3 N = normalize(vec3(model * vec4(aNormal, 0.f)));
        vec3 tan = normalize(aTangent);
        vec3 T = normalize(vec3(model * vec4(tan, 0.f)));
        //vec3 bitan = normalize(aBitangent);
        //vec3 B = normalize(vec3(model * vec4(bitan, 0.f)));
        vec3 B = cross(N, T);
        Tbn = mat3(T, B, N);
        //Tbn = mat3(model * vec4(aTangent, 0.f), model * vec4(bitangent, 0.f), model * vec4(aNormal, 0.f));
    }
    else
    {
        mat3 normalRecalculationMatrix = transpose(inverse(mat3(model)));
        Normal = normalRecalculationMatrix * aNormal;
    }

    mat3 m = transpose(inverse(mat3(model)));
    Tangent = m * aTangent;

    gl_Position = projection * view * worldPos;
}
