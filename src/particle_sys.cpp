#include <random>

#include "glm/gtc/quaternion.hpp"
#include "particle_sys.h"
#include "scene.h"
#include "log.h"
#include "random.h"

ParticleData::ParticleData(MeshWithMaterial& mesh, glm::vec3 velocity, float lifetime) :
    mesh(mesh), velocity(velocity), lifetime(lifetime)
{
    initialTransparency = ((TransparentMaterial*)mesh.material)->resourceData.tintAndOpacity.w;
}

ParticleSys::ParticleSys(std::vector<MeshWithMaterial*> meshes, AABB spawnBounds, glm::vec4 color, int count, float lifetime, float velocity) :
    spawnBounds(spawnBounds), color(color), maxParticleLifetime(lifetime), initialVelocity(velocity)
{
    glm::vec3 spawnZoneExtents = glm::abs(spawnBounds.max - spawnBounds.min) * 0.5f;
    glm::vec3 spawnCenter = spawnBounds.min + spawnZoneExtents;
    for (int i = 0; i < count; i++)
    {
        ParticleData particle(*meshes[i], glm::vec3(0.f), 0.f);
        particles.push_back(particle);
    }
}

void ParticleSys::Update(float dt, Transform& cameraTransform)
{
    glm::vec3 spawnZoneExtents = glm::abs(spawnBounds.max - spawnBounds.min) * 0.5f;
    glm::vec3 spawnCenter = spawnBounds.min + spawnZoneExtents;
    for (auto& particle : particles)
    {
        particle.lifetime -= dt;

        if (particle.lifetime <= 0.f)
        {
            particle.lifetime = RandomFloat() * maxParticleLifetime;
            particle.mesh.mesh.transform.pos = spawnCenter + glm::vec3(spawnZoneExtents.x * (RandomFloat() - 0.5f) * 2.f, 0.f,
                    spawnZoneExtents.z * (RandomFloat() - 0.5f) * 2.f);

            glm::vec3 fromCenter = (particle.mesh.mesh.transform.pos - spawnCenter) * glm::vec3(RandomFloat(), 1.f, RandomFloat());
            fromCenter = glm::normalize(fromCenter);
            fromCenter.y += 2.f;
            fromCenter = glm::normalize(fromCenter);

            particle.velocity = fromCenter * initialVelocity;
        }
        else
        {
            float yDt = particle.velocity.y * dt;
            ((TransparentMaterial*)particle.mesh.material)->resourceData.tintAndOpacity.w = particle.lifetime / maxParticleLifetime *
                particle.initialTransparency;

            particle.mesh.mesh.transform.pos += particle.velocity * dt;
            particle.velocity.x *= 1.f - dt;
            particle.velocity.y -= yDt * 0.34f;
            particle.velocity.z *= 1.f - dt;
        }

        particle.mesh.mesh.transform.rot = cameraTransform.rot;
    }
}
