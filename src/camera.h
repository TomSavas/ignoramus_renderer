#pragma once

#include "GLFW/glfw3.h"
#include "glm/glm.hpp"

#include "transform.h"

struct Camera
{
    Camera(float verticalFOV = 45.f, float aspectRatio = 16.f/9.f);

    void Update(GLFWwindow *window);

    void SetVerticalFOV(float radians);
    void SetAspectRatio(float ratio);
    void SetNearClippingPlane(float distanceFromCamera);
    void SetFarClippingPlane(float distanceFromCamera);

    glm::mat4 View();
    glm::mat4 MVP(const glm::mat4 &model);

    float verticalFOV;
    float aspectRatio;

    float nearClippingPlane;
    float farClippingPlane;

    glm::mat4 projection;

    Transform transform;
};
