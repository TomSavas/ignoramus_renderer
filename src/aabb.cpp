#include "aabb.h"

bool AABB::ViewFrustumIntersect(glm::mat4 frustumTransform)
{
    glm::vec4 aabbCorners[] =
    {
        glm::vec4(min.x, min.y, min.z, 1.f),
        glm::vec4(min.x, min.y, max.z, 1.f),
        glm::vec4(min.x, max.y, min.z, 1.f),
        glm::vec4(min.x, max.y, max.z, 1.f),
        glm::vec4(max.x, min.y, min.z, 1.f),
        glm::vec4(max.x, min.y, max.z, 1.f),
        glm::vec4(max.x, max.y, min.z, 1.f),
        glm::vec4(max.x, max.y, max.z, 1.f),
    };

#define X_BOUND 0 
#define Y_BOUND 1 
#define Z_BOUND 2 
#define BOUND_COUNT 8
    int behindBound[]  = {0, 0, 0};
    int infrontBound[] = {0, 0, 0};
    for (int i = 0; i < BOUND_COUNT; i++)
    {
        glm::vec4 corner = frustumTransform * aabbCorners[i];

        behindBound[X_BOUND]  += corner.x < -corner.w;
        infrontBound[X_BOUND] += corner.x >  corner.w;
        behindBound[Y_BOUND]  += corner.y < -corner.w;
        infrontBound[Y_BOUND] += corner.y >  corner.w;
        behindBound[Z_BOUND]  += corner.z < -0;
        infrontBound[Z_BOUND] += corner.z >  corner.w;
    }

    return infrontBound[X_BOUND] == BOUND_COUNT || behindBound[X_BOUND] == BOUND_COUNT || 
           infrontBound[Y_BOUND] == BOUND_COUNT || behindBound[Y_BOUND] == BOUND_COUNT || 
           infrontBound[Z_BOUND] == BOUND_COUNT || behindBound[Z_BOUND] == BOUND_COUNT;
}

glm::vec3 AABB::Extents()
{
    return (max - min) * 0.5f;
}
