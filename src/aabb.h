#pragma once 

#include <vector>

#include "glm/glm.hpp"

struct AABB 
{
    glm::vec3 min;
    glm::vec3 max;
    AABB(glm::vec3 min = glm::vec3(0, 0, 0), glm::vec3 max = glm::vec3(0, 0, 0)) : min(min), max(max) {}

    bool ViewFrustumIntersect(glm::mat4 viewProjection);
};
