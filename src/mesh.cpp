#include <cmath>

#include "log.h"
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

    glm::fvec4 *tangents = (glm::fvec4*) malloc(sizeof(glm::fvec4) * mesh.Vertices.size());
    glm::fvec4 *bitangents = (glm::fvec4*) malloc(sizeof(glm::fvec4) * mesh.Vertices.size());
    assert(tangents != nullptr);

    for (int i = 0; i < mesh.Vertices.size(); i++)
    {
        tangents[i] = glm::fvec4(0.f, 0.f, 0.f, 0.f);
        bitangents[i] = glm::fvec4(0.f, 0.f, 0.f, 0.f);
    }

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
            if (!std::isfinite(f))
            {
                f = 1.f;
            }

            glm::vec3 tangent;
            tangent.x = f * (deltaUV1.y * edge0.x - deltaUV0.y * edge1.x);
            tangent.y = f * (deltaUV1.y * edge0.y - deltaUV0.y * edge1.y);
            tangent.z = f * (deltaUV1.y * edge0.z - deltaUV0.y * edge1.z);

            glm::vec3 bitangent;
            bitangent.x = f * (-deltaUV1.x * edge0.x + deltaUV0.x * edge1.x);
            bitangent.y = f * (-deltaUV1.x * edge0.y + deltaUV0.x * edge1.y);
            bitangent.z = f * (-deltaUV1.x * edge0.z + deltaUV0.x * edge1.z);

            bool tangentNan = !std::isfinite(tangent.x) || !std::isfinite(tangent.y) || !std::isfinite(tangent.z);
            bool bitangentNan = !std::isfinite(bitangent.x) || !std::isfinite(bitangent.y) || !std::isfinite(bitangent.z);
            if (tangentNan != bitangentNan)
            {
                if (tangentNan)
                {
                    tangent = glm::cross(ToVec3(v0.Normal), bitangent);
                }
                else
                {
                    bitangent = glm::cross(tangent, ToVec3(v0.Normal));
                }
            }

            tangents[mesh.Indices[i + ((j+0) % 3)]] += glm::vec4(tangent, 0.f);
            tangents[mesh.Indices[i + ((j+1) % 3)]] += glm::vec4(tangent, 0.f);
            tangents[mesh.Indices[i + ((j+2) % 3)]] += glm::vec4(tangent, 0.f);

            bitangents[mesh.Indices[i + ((j+0) % 3)]] += glm::vec4(bitangent, 0.f);
            bitangents[mesh.Indices[i + ((j+1) % 3)]] += glm::vec4(bitangent, 0.f);
            bitangents[mesh.Indices[i + ((j+2) % 3)]] += glm::vec4(bitangent, 0.f);
        }
    }

    for (int i = 0; i < mesh.Vertices.size(); i++)
    {
        tangents[i] = glm::normalize(tangents[i]);
        bitangents[i] = glm::normalize(bitangents[i]);

        tangents[i].w = (glm::dot(glm::cross(glm::vec3(bitangents[i]), glm::vec3(tangents[i])), ToVec3(mesh.Vertices[i].Normal))) < 0.f ? -1.f : 1.f;
    }

    const int convertedVerticesElementCount = 3 + 3 + 4 + 2;
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
        convertedVertices[idx+ 9] = tangents[i].w;

        convertedVertices[idx+10] = mesh.Vertices[i].TextureCoordinate.X;
        convertedVertices[idx+11] = mesh.Vertices[i].TextureCoordinate.Y;

        aabbModelSpace.min = glm::vec3(glm::min(aabbModelSpace.min.x, mesh.Vertices[i].Position.X),
                glm::min(aabbModelSpace.min.y, mesh.Vertices[i].Position.Y),
                glm::min(aabbModelSpace.min.z, mesh.Vertices[i].Position.Z));
        aabbModelSpace.max = glm::vec3(glm::max(aabbModelSpace.max.x, mesh.Vertices[i].Position.X),
                glm::max(aabbModelSpace.max.y, mesh.Vertices[i].Position.Y),
                glm::max(aabbModelSpace.max.z, mesh.Vertices[i].Position.Z));
    }

    VertexBuffer vertexBuffer (convertedVertices, convertedVerticesElementCount * mesh.Vertices.size());
    IndexBuffer indexBuffer (mesh.Indices.data(), mesh.Indices.size());
    BufferLayout bufferLayout ({
            { "pos", 3, GL_FLOAT, sizeof(float) },
            { "normal", 3, GL_FLOAT, sizeof(float) },
            { "tangent", 4, GL_FLOAT, sizeof(float) },
            { "uv", 2, GL_FLOAT, sizeof(float) }});
    
    vertexArray = VertexArray(vertexBuffer, indexBuffer, bufferLayout);

    free(tangents);
    free(bitangents);
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

    // TEMP: quick lookup if the texture needs to be bound
    std::unordered_set<std::string> textureUniformNames;
    {
        int unused;
        GLenum type;
        char uniformName[128];
        int count = 0;
        glGetProgramiv(shader.id, GL_ACTIVE_UNIFORMS, &count);

        for (int i = 0; i < count; i++)
        {
            glGetActiveUniform(shader.id, i, sizeof(uniformName), &unused, &unused, &type, uniformName);

            bool isTexture = type == GL_SAMPLER_1D || type == GL_SAMPLER_2D || type == GL_SAMPLER_3D;
            if (!isTexture)
            {
                continue;
            }

            textureUniformNames.insert(std::string(uniformName));
        }
    }

    for (std::pair<std::string, std::string> nameToPath : textures)
    {
        if (!usingNormalMap && nameToPath.first.compare("tex_normal") == 0)
            usingNormalMap = true;

        if (textureUniformNames.find(nameToPath.first) == textureUniformNames.end())
        {
            continue;
        }

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
