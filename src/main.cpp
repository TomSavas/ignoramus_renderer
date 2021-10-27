#include <stdio.h>

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
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

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
    std::vector<NamedPipeline> pipelines = TestPipelines(shaders);
    int activePipelineIndex = 0;

    glClearColor(0.2f, 0.2f, 0.3f, 1.f);
    while (!glfwWindowShouldClose(window)) 
    {
        scene.camera.Update(window);
        scene.mainCameraParams.pos = scene.camera.transform.pos;
        scene.mainCameraParams.view = scene.camera.View();
        scene.mainCameraParams.projection = scene.camera.projection;

        shaders.ReloadChangedShaders();

        glfwPollEvents();
        glClear(GL_COLOR_BUFFER_BIT);

        ImGuiWrapper::PreRender();

        ShowControls(window, pipelines, activePipelineIndex, scene, shaders);
        pipelines[activePipelineIndex].pipeline.Render(scene, shaders);
        ImGuiWrapper::Render();

        glfwSwapBuffers(window);
    }

    ImGuiWrapper::Deinit();
    glfwTerminate();

    return 0;
}
