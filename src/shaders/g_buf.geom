/*
#version 330 core
layout (triangles) in;
layout (triangle_strip, max_vertices=3) out;

//out vec3 Barycentric; // FragPos from GS (output per emitvertex)

void main()
{
    Barycentric = vec3(1.f, 0.f, 0.f);
    gl_Position = gl_in[0].gl_Position;
    EmitVertex();
    Barycentric = vec3(0.f, 1.f, 0.f);
    gl_Position = gl_in[1].gl_Position;
    EmitVertex();
    Barycentric = vec3(0.f, 0.f, 1.f);
    gl_Position = gl_in[2].gl_Position;
    EmitVertex();

    EndPrimitive();
}  
*/

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
