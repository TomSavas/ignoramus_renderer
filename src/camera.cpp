#include <stdio.h>
#include <algorithm>

#include "camera.h"

#include "glm/gtx/transform.hpp"

Camera::Camera(float verticalFOV,
        float aspectRatio)
    : verticalFOV(verticalFOV), aspectRatio(aspectRatio),
      nearClippingPlane(0.1f), farClippingPlane(100000.f)
{
    projection = glm::perspective(glm::radians(verticalFOV), aspectRatio, nearClippingPlane, farClippingPlane);
    //projection = glm::ortho(0.f, 1920.f, 0.f, 1080.f, -1.f, farClippingPlane);
}

void Camera::Update(GLFWwindow *window)
{
    // adapt ue4's pie controls

    // looking
    static glm::dvec2 lastMousePos = glm::vec2(-1.f -1.f);
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS)
    {
        static double radToVertical = .0;
        static double radToHorizon = .0;

        if (lastMousePos.x == -1.f)
            glfwGetCursorPos(window, &lastMousePos.x, &lastMousePos.y);

        glm::dvec2 mousePos;
        glfwGetCursorPos(window, &mousePos.x, &mousePos.y);
        glm::dvec2 mousePosDif = mousePos - lastMousePos;
        lastMousePos = mousePos;

        radToVertical += mousePosDif.x * 0.004f;
        while(radToVertical > glm::pi<float>() * 2)
            radToVertical -= glm::pi<float>() * 2;
        while(radToVertical < -glm::pi<float>() * 2)
            radToVertical += glm::pi<float>() * 2;

        radToHorizon += mousePosDif.y * 0.005f;
        radToHorizon = std::min(std::max(radToHorizon, -glm::pi<double>() / 2 + 0.01), glm::pi<double>() / 2 - 0.01);

        transform.rot = glm::quat(glm::vec3(radToHorizon, -radToVertical, 0.f));
    }
    else
    {
        glfwGetCursorPos(window, &lastMousePos.x, &lastMousePos.y);
    }

    // movement
    glm::vec3 moveDir(0.f, 0.f, 0.f);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        moveDir += transform.Forward();
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        moveDir -= transform.Forward();

    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        moveDir += transform.Right();
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        moveDir -= transform.Right();

    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        moveDir += transform.Up();
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        moveDir -= transform.Up();

    moveDir *= glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ? 10.f : 4.f;

    transform.pos += moveDir;
}

void Camera::SetVerticalFOV(float radians) 
{
    verticalFOV = radians;
    projection = glm::perspective(verticalFOV, aspectRatio, nearClippingPlane, farClippingPlane);
}

void Camera::SetAspectRatio(float ratio)
{
    aspectRatio = ratio;
    projection = glm::perspective(verticalFOV, aspectRatio, nearClippingPlane, farClippingPlane);
}

void Camera::SetNearClippingPlane(float distanceFromCamera)
{
    nearClippingPlane = distanceFromCamera;
    projection = glm::perspective(verticalFOV, aspectRatio, nearClippingPlane, farClippingPlane);
}

void Camera::SetFarClippingPlane(float distanceFromCamera)
{
    farClippingPlane = distanceFromCamera;
    projection = glm::perspective(verticalFOV, aspectRatio, nearClippingPlane, farClippingPlane);
}

glm::mat4 Camera::View() 
{
    return glm::lookAt(transform.pos, transform.pos + transform.Forward(), transform.Up());
}

glm::mat4 Camera::MVP(const glm::mat4 &model)
{
    return projection * View() * model;
}
