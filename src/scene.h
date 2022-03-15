#pragma once

#include <stdint.h>

#include "glm/glm.hpp"

#include "camera.h"
#include "mesh.h"
#include "material.h"
#include "aabb.h"
#include "render_pipeline.h"

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

    // Empty of geometry or shaders, just stores global attachments
    Renderpass globalAttachments;

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

    struct DirectionalLight
    {
        glm::vec3 color;
        Transform transform;

        glm::mat4 viewProjection = glm::mat4();
    } directionalLight;

    struct PointLight
    {
        glm::vec4 color;
        glm::vec4 pos;
        glm::vec4 radius;

        PointLight(glm::vec3 color, glm::vec3 pos, float radius) : color(glm::vec4(color, 0.f)), pos(glm::vec4(pos, 0.f)), radius(glm::vec4(radius)) {}// , radius(radius) {}
        PointLight(glm::vec4 color = glm::vec4(1.f, 1.f, 1.f, 0.f), glm::vec4 pos = glm::vec4(0, 0, 0, 0), float radius = 1.f) : color(color), pos(pos) {}//, radius(radius) {}
    };

#define MAX_POINT_LIGHTS 16348
    struct Lights
    {
        PointLight pointLights[MAX_POINT_LIGHTS];
        int pointLightCount = 0;
    } lights;

    struct Lighting
    {
        glm::mat4 directionalLightViewProjection = glm::mat4();
        glm::vec4 directionalLightColor;
        glm::vec4 directionalLightDir;

        glm::vec4 directionalBiasAndAngleBias;
    } lighting;
    unsigned int lightingUboId;

    Scene();

    void BindSceneParams();
    void BindCameraParams();
    void BindLighting();
};
