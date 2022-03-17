#pragma once

#include "glm/glm.hpp"
#include "aabb.h"
#include "transform.h"

struct MeshWithMaterial;
struct ParticleData
{
    // Temporary. We should be instancing
    MeshWithMaterial& mesh;
    glm::vec3 velocity;
    float lifetime;
    float initialTransparency;

    ParticleData(MeshWithMaterial& mesh, glm::vec3 velocity, float lifetime);
};

struct ParticleSys
{
    AABB spawnBounds;
    float initialVelocity;
    glm::vec4 color;

    float maxParticleLifetime;
    std::vector<ParticleData> particles;

    ParticleSys(std::vector<MeshWithMaterial*> meshes, AABB spawnBounds, glm::vec4 color, int count, float maxLifetime, float velocity);
    void Update(float dt, Transform& cameraTransform);
};
