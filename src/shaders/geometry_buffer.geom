#version 330 core
layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

in vec3 vWorldFragPos[];
in vec4 vFragPos[];
in vec2 vTexCoords[];
in vec3 vNormal[];
in mat3 vTbn[];
in vec3 vTangent[];

out vec3 WorldFragPos;
out vec4 FragPos;
out vec2 TexCoords;
out vec3 Normal;
out mat3 Tbn;
out vec3 Tangent;

out vec3 Barycentric;

void main() {    
    vec3 VertexCoords[3];
    VertexCoords[0] = vec3(1.f, 0.f, 0.f);
    VertexCoords[1] = vec3(0.f, 1.f, 0.f);
    VertexCoords[2] = vec3(0.f, 0.f, 1.f);

    for (int i = 0; i < 3; i++)
    {
        gl_Position = gl_in[i].gl_Position; 

        WorldFragPos = vWorldFragPos[i];
        FragPos = vFragPos[i];
        TexCoords = vTexCoords[i];
        Normal = vNormal[i];
        Tbn = vTbn[i];
        Tangent = vTangent[i];

        Barycentric = VertexCoords[i];

        EmitVertex();
    }
    EndPrimitive();
}  
