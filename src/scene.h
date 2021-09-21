#pragma once

#include <stdint.h>

#include "glm/glm.hpp"

#include "mesh.h"
#include "material.h"

// TODO: future optimization for instancing
/*
struct MeshInstance
{
    uint32 meshIndex;
    Transform *transform;
    Material material;
};
*/

// TODO: yes this is idiotic
struct MeshWithMaterial
{
    Mesh mesh;
    Material* material;
};

struct Scene
{
    std::unordered_map<MeshTag, std::vector<MeshWithMaterial>> meshes;

    struct SceneParams
    {
        int pixelSize;
        int wireframe;
        float gamma;
    } sceneParams;
    unsigned int sceneParamsUboId;       

    struct CameraParams
    {
        glm::mat4 view;
        glm::mat4 projection;
        glm::vec3 pos;
    } mainCameraParams;
    unsigned int mainCameraParamsUboId;       

    struct Lighting
    {
        int usingShadowCubemap = false;
        int usingShadowMap = false;

        glm::vec3 pointLightPos = glm::vec3();
        glm::vec3 directionalLightPos = glm::vec3();

        glm::mat4 directionalLightViewProjection = glm::mat4();

        float directionalBias = 0.f;
        float directionalAngleBias = 0.f;

        float pointBias = 0.f;
        float pointAngleBias = 0.f;
    } lighting;
    unsigned int lightingUboId;

    Scene();

    void BindSceneParams();
    void BindCameraParams();
    void BindLighting();
};
