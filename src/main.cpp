#include <algorithm>
#include <cmath>
#include <random>
#include <stdio.h>

#include <glm/gtx/string_cast.hpp>

#include "imgui_wrapper.h"
#include "scene.h"
#include "test_structures.h"
#include "log.h"

void GLLog(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam);
void ShowControls(GLFWwindow* window, std::vector<NamedPipeline>& pipelines, int& activePipelineIndex, Scene& scene, ShaderPool& shaders);

int main(void) 
{
    if (!glfwInit())
    {
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);

    GLFWwindow *window = glfwCreateWindow(1920, 1080, "Ignoramus", NULL, NULL);
    if (!window) 
    {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    if (glewInit() != GLEW_OK)
    {
        fprintf(stderr, "Failed initializing glew\n");
        glfwTerminate();
        return -1;
    }

    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(GLLog, 0);

    ImGuiWrapper::Init(window);
    // NOTE: Fix my harware issue, for a phantom controller always holding down on one joystick
    ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad; 

    ShaderPool shaders;
    Scene scene = TestScene();
    std::vector<NamedPipeline> pipelines = TestPipelines(scene.globalAttachments, shaders);
    int activePipelineIndex = 0;

    glfwSwapInterval(0.f);

    glClearColor(0.2f, 0.2f, 0.3f, 1.f);
    while (!glfwWindowShouldClose(window)) 
    {
        scene.camera.Update(window);
        scene.mainCameraParams.nearFarPlanes = glm::vec4(scene.camera.nearClippingPlane, scene.camera.farClippingPlane, 0.f, 0.f);
        scene.mainCameraParams.pos = glm::vec4(scene.camera.transform.pos, 0.f);
        scene.mainCameraParams.view = scene.camera.View();
        scene.mainCameraParams.projection = scene.camera.projection;
        scene.mainCameraParams.viewProjection = scene.mainCameraParams.projection * scene.mainCameraParams.view;

        // Move point lights around the centre in an elipse
        for (int i = 0; i < MAX_POINT_LIGHTS; i++)
        {
            float d = sqrt(scene.lights.pointLights[i].pos.x * scene.lights.pointLights[i].pos.x + scene.lights.pointLights[i].pos.z * scene.lights.pointLights[i].pos.z * 4.f);
            float angle = atan2(scene.lights.pointLights[i].pos.z * 2.f, scene.lights.pointLights[i].pos.x);
            angle += 0.01;
            scene.lights.pointLights[i].pos.x = cos(angle) * d;
            scene.lights.pointLights[i].pos.z = sin(angle) * d / 2.f;
        }

        // Update particle system
        for (auto& particleSys : scene.particleSystems)
        {
            //particleSys.Update(0.016f, scene.camera.transform.pos, scene.camera.transform.Up());   
            particleSys.Update(0.016f, scene.camera.transform);   
        }

        // TODO: remove, temporarily here for moving the directional light manually
        if (glfwGetKey(window, GLFW_KEY_H) == GLFW_PRESS)
        {
            scene.directionalLight.transform = scene.camera.transform;

            glm::mat4 proj = glm::ortho(-3000.f, 3000.f, -1000.f, 8000.f, scene.camera.nearClippingPlane, scene.camera.farClippingPlane);
            scene.directionalLight.viewProjection = proj * scene.camera.View();

            scene.lighting.directionalLightDir = glm::vec4(scene.directionalLight.transform.Forward(), 0.f);
            scene.lighting.directionalLightViewProjection = scene.directionalLight.viewProjection;
        }

        shaders.ReloadChangedShaders();

        glfwPollEvents();
        glClear(GL_COLOR_BUFFER_BIT);

        ImGuiWrapper::PreRender();

        ShowControls(window, pipelines, activePipelineIndex, scene, shaders);

        // Sorting/shuffling to display issues with unsorted transparency
        static bool requiresShuffle = false;
        if (strcmp(pipelines[activePipelineIndex].name, "Sorted forward transparency") == 0)
        {
            std::vector<MeshWithMaterial>& transparentMeshes = scene.meshes[TRANSPARENT]; 
            std::sort(transparentMeshes.begin(), transparentMeshes.end(), 
                [&](const MeshWithMaterial& a, const MeshWithMaterial& b) -> bool
                {
                    return glm::distance(a.mesh.transform.pos, scene.camera.transform.pos) > glm::distance(b.mesh.transform.pos, scene.camera.transform.pos);
                });

            requiresShuffle = true;
        }
        if (strcmp(pipelines[activePipelineIndex].name, "Unsorted forward transparency") == 0 && requiresShuffle)
        {
            std::vector<MeshWithMaterial>& transparentMeshes = scene.meshes[TRANSPARENT]; 

            std::random_device rd;
            std::mt19937 g(rd());
            std::shuffle(transparentMeshes.begin(), transparentMeshes.end(), g);

            requiresShuffle = false;
        }

        pipelines[activePipelineIndex].pipeline.Render(scene, shaders);
        ImGuiWrapper::Render();

        glfwSwapBuffers(window);
    }

    ImGuiWrapper::Deinit();
    glfwTerminate();

    return 0;
}
