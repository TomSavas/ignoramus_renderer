#pragma once

#include <unordered_map>

#include "OBJ_Loader.h"

#include "shader.h"
#include "texture.h"
#include "vertex_array.h"
#include "transform.h"
#include "aabb.h"

enum MeshTag
{
    NONE            = 0x00,
    OPAQUE          = 0x01,
    TRANSPARENT     = 0x02,
    SCREEN_QUAD     = 0x04,
    COMPUTE         = 0x08,
    PARTICLE        = 0x10,

    ALL             = -1
};

struct Mesh
{
    MeshTag meshTag;
    VertexArray vertexArray;
    AABB aabbModelSpace;

    Mesh(objl::Mesh &mesh, MeshTag tag = OPAQUE, std::vector<std::pair<std::string, std::string>> overrideTexturesWithPaths = std::vector<std::pair<std::string, std::string>>());
    void Render(Shader &shader);

    std::unordered_map<std::string, std::string> textures;

    // TMP: should reference the model's transform
    Transform transform;

    static Mesh ScreenQuadMesh();
};
