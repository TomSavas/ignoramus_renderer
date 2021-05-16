#include <stdio.h>

#include <stdio.h>

#define GLX_GLXEXT_PROTOTYPES
#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include "GLFW/glfw3.h"

#include "imgui_wrapper.h"

#include "deferred_render_test.h"

//tmp
#include <glm/gtc/type_ptr.hpp>

void ShowFPS()
{
    static int frameCount = 0;

    static bool open = true;
    const float PAD = 10.0f;

    ImGuiIO& io = ImGui::GetIO();
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 work_pos = viewport->WorkPos; // Use work area to avoid menu-bar/task-bar, if any!
    ImVec2 work_size = viewport->WorkSize;
    ImVec2 window_pos, window_pos_pivot;
    window_pos.x = work_pos.x + work_size.x - PAD;
    window_pos.y = work_pos.y + PAD;
    window_pos_pivot.x = 1.0f;
    window_pos_pivot.y = 0.0f;
    ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
    window_flags |= ImGuiWindowFlags_NoMove;

#define GLX_RENDERER_VIDEO_MEMORY_MESA 0x8187
    unsigned int availableMem = 0;
    glXQueryCurrentRendererIntegerMESA(GLX_RENDERER_VIDEO_MEMORY_MESA, &availableMem);

    ImGui::SetNextWindowBgAlpha(0.35f); // Transparent background
    if (ImGui::Begin("Example: Simple overlay", &open, window_flags))
    {
        ImGui::Text("Ignoramus v.0.0.1\n");
        ImGui::Separator();

        static const unsigned char *vendor = glGetString(GL_VENDOR);
        static const unsigned char *renderer = glGetString(GL_RENDERER);
        static const unsigned char *version = glGetString(GL_VERSION);
        ImGui::Text("%s\n%s\n%s", vendor, renderer, version);

        ImGui::Separator();
        ImGui::Text("Frametime: %.2fms", io.DeltaTime * 1000);
        ImGui::Text("FPS: %.1f", io.Framerate);
        ImGui::Separator();
        ImGui::Text("MESA available VRAM: %u MB", availableMem);

    }
    ImGui::End();
}

int main(void) 
{
    if (!glfwInit())
        return -1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
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

    ImGuiWrapper::Init(window);

    DeferredTest test;
    test.window = window;

    glClearColor(0.2f, 0.2f, 0.3f, 1.f);
    bool showDemoWindow = true;
    while (!glfwWindowShouldClose(window)) 
    {
        glfwPollEvents();
        glClear(GL_COLOR_BUFFER_BIT);

        ImGuiWrapper::PreRender();
        test.camera.Update(window);

        if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS)
            test.model = new Model("../assets/sponza/sponza.obj");

        ShowFPS();

        test.Render();
        ImGuiWrapper::Render();

        /*
        // save previous matrix
        glPushMatrix();
        // clear matrix
        glLoadIdentity();
        // apply rotations

        glm::mat4 rot = glm::toMat4(test.camera.transform.rot);
        glLoadMatrixf((float*)glm::value_ptr(rot));

        // move the axes to the screen corner
        // draw our axes
        glBegin(GL_LINES);
        // draw line for x axis
        glColor3f(1.0, 0.0, 0.0);
        glVertex3f(0.0, 0.0, 0.0);
        glVertex3f(1.0, 0.0, 0.0);
        // draw line for y axis
        glColor3f(0.0, 1.0, 0.0);
        glVertex3f(0.0, 0.0, 0.0);
        glVertex3f(0.0, 1.0, 0.0);
        // draw line for Z axis
        glColor3f(0.0, 0.0, 1.0);
        glVertex3f(0.0, 0.0, 0.0);
        glVertex3f(0.0, 0.0, 1.0);
        glEnd();
        // load the previous matrix
        glPopMatrix();
        */

        glfwSwapBuffers(window);
    }

    //ImGuiWrapper::Deinit();
    glfwTerminate();

    return 0;
}
