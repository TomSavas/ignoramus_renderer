#pragma once

#include <unordered_map>

#include "OBJ_Loader.h"

#include "shader.h"
#include "texture.h"
#include "vertex_array.h"

struct Mesh
{
    VertexArray vertexArray;

    Mesh(objl::Mesh &mesh, std::vector<std::pair<std::string, std::string>> overrideTexturesWithPaths = std::vector<std::pair<std::string, std::string>>());
    void Render(Shader &shader);

    //std::unordered_map<std::string, *Texture> textures;
    std::unordered_map<std::string, std::string> textures;
};
