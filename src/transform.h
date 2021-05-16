#pragma once

#include "glm/glm.hpp"
#include "glm/gtx/quaternion.hpp"

struct Transform
{
    glm::vec3 pos;
    glm::quat rot;
    glm::vec3 scale;

    Transform(glm::vec3 pos = glm::vec3(0.f, 0.f, 0.f), glm::quat rot = glm::quat(glm::vec3(0.f, 0.f, 0.f)), glm::vec3 scale = glm::vec3(1.f, 1.f, 1.f));

    glm::mat4 Model();

    glm::vec3 Forward();
    glm::vec3 Up();
    glm::vec3 Right();
};
