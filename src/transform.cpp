#include "transform.h"

#include "glm/gtx/transform.hpp"

Transform::Transform(glm::vec3 pos, glm::quat rot, glm::vec3 scale)
    : pos(pos), rot(rot), scale(scale)
{
}

glm::mat4 Transform::Model()
{
    return glm::translate(pos) * toMat4(rot) * glm::scale(glm::mat4(1), scale);
}

glm::vec3 Transform::Forward()
{
    return toMat4(rot) * glm::vec4(0.f, 0.f, -1.f, 0.f);
}

glm::vec3 Transform::Up()
{
    return toMat4(rot) * glm::vec4(0.f, 1.f, 0.f, 0.f);
}

glm::vec3 Transform::Right()
{
    return toMat4(rot) * glm::vec4(1.f, 0.f, 0.f, 0.f);
}
