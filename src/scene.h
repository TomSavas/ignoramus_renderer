#pragma once

#include <stdint.h>

#include "glm/glm.hpp"

#include "camera.h"
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
    bool disableOpaque = false;
    Camera camera;

    struct SceneParams
    {
        int pixelSize;
        int wireframe;
        float gamma;
        float specularPower;
        float viewportWidth;
        float viewportHeight;
    } sceneParams;
    unsigned int sceneParamsUboId;       

    struct CameraParams
    {
        glm::mat4 view;
        glm::mat4 projection;
        glm::mat4 viewProjection;
        glm::vec4 pos;
        glm::vec4 nearFarPlanes;
    } mainCameraParams;
    unsigned int mainCameraParamsUboId;       

    // TODO: extend to multiple lights
    struct DirectionalLight
    {
        glm::vec3 color;
        Transform transform;

        glm::mat4 viewProjection = glm::mat4();
        // OBB volume
    } directionalLight;

    struct Lighting
    {
        glm::mat4 directionalLightViewProjection = glm::mat4();
        glm::vec4 directionalLightColor;
        glm::vec4 directionalLightDir;

        glm::vec4 directionalBiasAndAngleBias;
    } lighting;
    unsigned int lightingUboId;

    struct PointLight
    {
        glm::vec3 color;
        glm::vec3 pos;
        glm::vec3 radius;
    };

    Scene();

    void BindSceneParams();
    void BindCameraParams();
    void BindLighting();
};
