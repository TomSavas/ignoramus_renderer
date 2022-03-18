#include <limits>

#include "model.h"

Model::Model(const char *filepath, std::vector<std::pair<std::string, std::string>> texturesWithPaths)
{
    objl::Loader loader;
    loader.LoadFile(filepath);

    aabbModelSpace = AABB(glm::vec3(std::numeric_limits<float>::max()), glm::vec3(std::numeric_limits<float>::min()));
    for (int i = 0; i < loader.LoadedMeshes.size(); i++)
    {
        meshes.push_back(Mesh(loader.LoadedMeshes[i], OPAQUE, texturesWithPaths));
        aabbModelSpace.min = glm::min(aabbModelSpace.min, meshes[i].aabbModelSpace.min);
        aabbModelSpace.max = glm::max(aabbModelSpace.max, meshes[i].aabbModelSpace.max);
    }
}

Model::Model(objl::Mesh mesh, std::vector<std::pair<std::string, std::string>> texturesWithPaths)
{
    meshes.push_back(mesh);
}

void Model::Render(Shader &shader)
{
    for (Mesh &mesh : meshes)
        mesh.Render(shader);

    // Reset active texture
    glActiveTexture(GL_TEXTURE0);
}

///*static*/ Model Model::Quad(glm::vec3 scale, Texture &texture)
//{
//    static std::vector<unsigned int> indices = { 0, 3, 2, 0, 2, 1 };
//
//    std::vector<objl::Vertex> vertices = { 
//        { objl::Vector3(-scale.x / 2.0, -scale.y / 2.0, 0.f), objl::Vector3(0.f, 0.f, 1.f), objl::Vector2(0.f, 0.f) }, // front bot left
//        { objl::Vector3(-scale.x / 2.0,  scale.y / 2.0, 0.f), objl::Vector3(0.f, 0.f, 1.f), objl::Vector2(0.f, 1.f) }, // front top left
//        { objl::Vector3( scale.x / 2.0,  scale.y / 2.0, 0.f), objl::Vector3(0.f, 0.f, 1.f), objl::Vector2(1.f, 1.f) }, // front top right
//        { objl::Vector3( scale.x / 2.0, -scale.y / 2.0, 0.f), objl::Vector3(0.f, 0.f, 1.f), objl::Vector2(1.f, 0.f) }, // front bot right
//    };
//    objl::Mesh *mesh = new objl::Mesh(vertices, indices);   
//
//    return Model(*mesh, texture);
//}
//
/*static*/ Model *Model::DoubleSidedQuad(glm::vec3 scale)
{
    static std::vector<unsigned int> indices = { 
        0, 3, 2, 0, 2, 1,
        6, 7, 4, 5, 6, 4, // backside due to back face culling
    };

    std::vector<objl::Vertex> vertices = { 
        { objl::Vector3(-scale.x / 2.0, -scale.y / 2.0, 0.f), objl::Vector3(0.f, 0.f, 1.f), objl::Vector2(0.f, 0.f) }, // front bot left
        { objl::Vector3(-scale.x / 2.0,  scale.y / 2.0, 0.f), objl::Vector3(0.f, 0.f, 1.f), objl::Vector2(0.f, 1.f) }, // front top left
        { objl::Vector3( scale.x / 2.0,  scale.y / 2.0, 0.f), objl::Vector3(0.f, 0.f, 1.f), objl::Vector2(1.f, 1.f) }, // front top right
        { objl::Vector3( scale.x / 2.0, -scale.y / 2.0, 0.f), objl::Vector3(0.f, 0.f, 1.f), objl::Vector2(1.f, 0.f) }, // front bot right

        // We need separate vertices with correct normals for back side of the quad
        { objl::Vector3(-scale.x / 2.0, -scale.y / 2.0, 0.f), objl::Vector3(0.f, 0.f, -1.f), objl::Vector2(0.f, 0.f) }, // back bot left
        { objl::Vector3(-scale.x / 2.0,  scale.y / 2.0, 0.f), objl::Vector3(0.f, 0.f, -1.f), objl::Vector2(0.f, 1.f) }, // back top left
        { objl::Vector3( scale.x / 2.0,  scale.y / 2.0, 0.f), objl::Vector3(0.f, 0.f, -1.f), objl::Vector2(1.f, 1.f) }, // back top right
        { objl::Vector3( scale.x / 2.0, -scale.y / 2.0, 0.f), objl::Vector3(0.f, 0.f, -1.f), objl::Vector2(1.f, 0.f) }, // back bot right
    };
    objl::Mesh mesh = objl::Mesh(vertices, indices);   

    return new Model(mesh,
            std::vector<std::pair<std::string, std::string>> { 
                std::make_pair("tex_diffuse", "../assets/textures/default.jpeg"),
                std::make_pair("tex_normal", "../assets/textures/default.jpeg"),
                std::make_pair("tex_specular", "../assets/textures/default.jpeg") 
            });
}
