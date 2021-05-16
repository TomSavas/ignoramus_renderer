#ifndef MODEL_H
#define MODEL_H

#include <unordered_map>
#include <vector>
#include <string>

#include "OBJ_Loader.h"

#include "mesh.h"
#include "shader.h"
#include "texture.h"
#include "transform.h"
#include "vertex_array.h"

class Model
{
public:
    Model(const char *filepath, std::vector<std::pair<std::string, std::string>> texturesWithPaths = std::vector<std::pair<std::string, std::string>>());
    Model(objl::Mesh mesh, std::vector<std::pair<std::string, std::string>> texturesWithPaths = std::vector<std::pair<std::string, std::string>>());

    static Model *DoubleSidedQuad(glm::vec3 scale);

    void Render(Shader &shader);

    std::vector<Mesh> meshes;
    std::unordered_map<std::string, Texture> textures;
};

#endif
