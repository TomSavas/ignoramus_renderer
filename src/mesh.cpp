#include "mesh.h"

#include "texture_pool.h"

glm::fvec3 ToVec3(objl::Vector3 vec)
{
    return glm::fvec3(vec.X, vec.Y, vec.Z);
}

glm::fvec2 ToVec2(objl::Vector2 vec) 
{
    return glm::fvec2(vec.X, vec.Y);
}

Mesh::Mesh(objl::Mesh &mesh, MeshTag tag, std::vector<std::pair<std::string, std::string>> overrideTexturesWithPaths)
{
    meshTag = tag;

    glm::fvec3 *tangents = (glm::fvec3*) malloc(sizeof(glm::fvec3) * mesh.Vertices.size());
    assert(tangents != nullptr);

    for (int i = 0; i < mesh.Vertices.size(); i++)
        tangents[i] = glm::fvec3(0.f, 0.f, 0.f);

    assert(mesh.Indices.size() % 3 == 0);
    for (int i = 0; i < mesh.Indices.size(); i += 3)
    {
        for (int j = 0; j < 3; j++)
        {
            objl::Vertex v0 = mesh.Vertices[mesh.Indices[i + ((j+0) % 3)]];
            objl::Vertex v1 = mesh.Vertices[mesh.Indices[i + ((j+1) % 3)]];
            objl::Vertex v2 = mesh.Vertices[mesh.Indices[i + ((j+2) % 3)]];

            glm::vec3 edge0 = ToVec3(v1.Position) - ToVec3(v0.Position);
            glm::vec3 edge1 = ToVec3(v2.Position) - ToVec3(v0.Position);

            glm::vec2 deltaUV0 = ToVec2(v1.TextureCoordinate) - ToVec2(v0.TextureCoordinate);
            glm::vec2 deltaUV1 = ToVec2(v2.TextureCoordinate) - ToVec2(v0.TextureCoordinate);

            float f = 1.f / (deltaUV0.x * deltaUV1.y - deltaUV1.x * deltaUV0.y);
            glm::vec3 tangent;
            tangent.x = f * (deltaUV1.y * edge0.x - deltaUV0.y * edge1.x);
            tangent.y = f * (deltaUV1.y * edge0.y - deltaUV0.y * edge1.y);
            tangent.z = f * (deltaUV1.y * edge0.z - deltaUV0.y * edge1.z);

            tangents[mesh.Indices[i + ((j+0) % 3)]] += tangent;
            tangents[mesh.Indices[i + ((j+1) % 3)]] += tangent;
            tangents[mesh.Indices[i + ((j+2) % 3)]] += tangent;
        }
    }

    for (int i = 0; i < mesh.Vertices.size(); i++)
        tangents[i] = glm::normalize(tangents[i]);

    const int convertedVerticesElementCount = 3 + 3 + 3 + 2;
    float *convertedVertices = (float *) malloc(sizeof(float) * convertedVerticesElementCount * mesh.Vertices.size());
    assert(convertedVertices != nullptr);
    for (int i = 0; i < mesh.Vertices.size(); i++)
    {
        int idx = i * convertedVerticesElementCount;
        convertedVertices[idx+ 0] = mesh.Vertices[i].Position.X;
        convertedVertices[idx+ 1] = mesh.Vertices[i].Position.Y;
        convertedVertices[idx+ 2] = mesh.Vertices[i].Position.Z;

        convertedVertices[idx+ 3] = mesh.Vertices[i].Normal.X;
        convertedVertices[idx+ 4] = mesh.Vertices[i].Normal.Y;
        convertedVertices[idx+ 5] = mesh.Vertices[i].Normal.Z;

        // tangent/bitangent for TBN matrix
        convertedVertices[idx+ 6] = tangents[i].x;
        convertedVertices[idx+ 7] = tangents[i].y;
        convertedVertices[idx+ 8] = tangents[i].z;

        convertedVertices[idx+ 9] = mesh.Vertices[i].TextureCoordinate.X;
        convertedVertices[idx+10] = mesh.Vertices[i].TextureCoordinate.Y;
    }

    VertexBuffer vertexBuffer (convertedVertices, convertedVerticesElementCount * mesh.Vertices.size());
    IndexBuffer indexBuffer (mesh.Indices.data(), mesh.Indices.size());
    BufferLayout bufferLayout ({
            { "pos", 3, GL_FLOAT, sizeof(float) },
            { "normal", 3, GL_FLOAT, sizeof(float) },
            { "tangent", 3, GL_FLOAT, sizeof(float) },
            { "uv", 2, GL_FLOAT, sizeof(float) }});
    
    vertexArray = VertexArray(vertexBuffer, indexBuffer, bufferLayout);

    free(tangents);
    free(convertedVertices);

    for (int i = 0; i < overrideTexturesWithPaths.size(); i++)
    {
        if (overrideTexturesWithPaths[i].second.empty())
            continue;

        std::string path = "../assets/" + overrideTexturesWithPaths[i].second;
        GetTexture(path);
        textures[overrideTexturesWithPaths[i].first] = path;
    }

    if (textures.find("tex_diffuse") == textures.end() && !mesh.MeshMaterial.map_Kd.empty())
    {
        std::string path = "../assets/" + mesh.MeshMaterial.map_Kd;
        GetTexture(path);
        textures["tex_diffuse"] = path;
    }
    if (textures.find("tex_normal") == textures.end() && !mesh.MeshMaterial.map_bump.empty())
    {
        std::string path = "../assets/" + mesh.MeshMaterial.map_bump;
        GetTexture(path);
        textures["tex_normal"] = path;
    }
    if (textures.find("tex_specular") == textures.end() && !mesh.MeshMaterial.map_Ks.empty())
    {
        std::string path = "../assets/" + mesh.MeshMaterial.map_Ks;
        GetTexture(path);
        textures["tex_specular"] = path;
    }
}

void Mesh::Render(Shader &shader)
{
    int i = 0;
    // Hmm, there should be some sort of communication between the shader and model here.
    // Shader dictates what textures it needs, the model provides them
    bool usingNormalMap = false;
    for (std::pair<std::string, std::string> nameToPath : textures)
    {
        if (!usingNormalMap && nameToPath.first.compare("tex_normal") == 0)
            usingNormalMap = true;

        Texture *t = GetTexture(nameToPath.second);

        glActiveTexture(GL_TEXTURE0 + i);
        shader.SetUniform(nameToPath.first.c_str(), i++);
        t->Bind();
    }
    shader.SetUniform("usingNormalMap", usingNormalMap);

    vertexArray.Bind();
    glDrawElements(GL_TRIANGLES, vertexArray.GetIndexCount(), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

/*static*/ Mesh Mesh::ScreenQuadMesh()
{
    float vertices[] = {
        // pos           // normals     // uvs
        -1.f,  1.f, 0.f, 0.f, 0.f, 1.f, 0.f, 1.f,
        -1.f, -1.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f,
         1.f,  1.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f,
         1.f, -1.f, 0.f, 0.f, 0.f, 1.f, 1.f, 0.f,
    };

    objl::Mesh mesh;
    mesh.MeshName = "screen quad";
    for (int i = 0; i < 4; i++)
    {
        mesh.Vertices.push_back(
            {
                objl::Vector3(vertices[i*8 + 0], vertices[i*8 + 1], vertices[i*8 + 2]),
                objl::Vector3(vertices[i*8 + 3], vertices[i*8 + 4], vertices[i*8 + 5]),
                objl::Vector2(vertices[i*8 + 6], vertices[i*8 + 7])
            });
    }
    mesh.Indices = 
    {
        1, 2, 0,
        2, 1, 3 
    };

    return Mesh(mesh, SCREEN_QUAD);
}
