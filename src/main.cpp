#include <stdio.h>

#include <stdio.h>

#define GLX_GLXEXT_PROTOTYPES
#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include "GLFW/glfw3.h"

#include "imgui_wrapper.h"

#include "deferred_render_test.h"
#include "scene.h"

//tmp
#include <glm/gtc/type_ptr.hpp>

#include "log.h"

void GLAPIENTRY
MessageCallback(GLenum source,
        GLenum type,
        GLuint id,
        GLenum severity,
        GLsizei length,
        const GLchar* message,
        const void* userParam)
{
    LOG_ERROR("OpenGL", "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s",
            ( type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : "" ),
            type, severity, message );
}

void ShowFPS(DeferredTest &test)
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
        ImGui::Text("Ignoramus v.0.0.1");
        ImGui::SameLine();
#ifdef DEBUG
        ImGui::TextColored(ImVec4(1.f, 0.f, 0.f, 1.f), "DEBUG");
#else
        ImGui::TextColored(ImVec4(0.f, 1.f, 0.f, 1.f), "RELEASE");
#endif
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

        ImGui::Separator();

#define VBO_FREE_MEMORY_ATI          0x87FB
#define TEXTURE_FREE_MEMORY_ATI      0x87FC
#define RENDERBUFFER_FREE_MEMORY_ATI 0x87FD
        GLint vbo[4] = {0, 0, 0, 0};
        GLint tex[4] = {0, 0, 0, 0};
        GLint renderbuf[4] = {0, 0, 0, 0};
        glGetIntegerv(VBO_FREE_MEMORY_ATI, vbo);
        glGetIntegerv(TEXTURE_FREE_MEMORY_ATI, tex);
        glGetIntegerv(RENDERBUFFER_FREE_MEMORY_ATI, renderbuf);

        // Kinda useless, displays the complete amount of used vram
        ImGui::Text("ATI VBO VRAM      ATI TEXTURE VRAM    ATI RENDERBUFFER RAM");
        ImGui::Text("TOTAL: %4u MB    TOTAL: %4u MB      TOTAL: %4u MB", vbo[2] / 1024, tex[2] / 1024, renderbuf[2] / 1024);
        ImGui::Text(" USED: %4u MB     USED: %4u MB       USED: %4u MB", (vbo[2] - vbo[0]) / 1024, (tex[2] - tex[0]) / 1024, (renderbuf[2] - renderbuf[0]) / 1024);
        ImGui::Text(" FREE: %4u MB     FREE: %4u MB       FREE: %4u MB", vbo[0] / 1024, tex[0] / 1024, renderbuf[0] / 1024);

        ImGui::Separator();
        ImGui::Text("Camera position: %.4f, %.4f, %.4f", test.camera.transform.pos.x, test.camera.transform.pos.y, test.camera.transform.pos.z);
    }
    ImGui::End();
}

void ShowSettings(DeferredTest& test, Scene& scene)
{
    static bool open = true;
    if (ImGui::Begin("Settings"), open)
    {
        ImGui::Text("Pixel size");
        ImGui::SameLine();
        ImGui::SliderInt("##Pixel size", &scene.sceneParams.pixelSize, 1, 50);

        ImGui::Text("Wireframe");
        ImGui::SameLine();
        ImGui::Checkbox("##wireframe", (bool*)&scene.sceneParams.wireframe);

        static bool vsync = true;
        ImGui::Text("VSync");
        ImGui::SameLine();
        if (ImGui::Checkbox("##vsync", &vsync))
            glfwSwapInterval(vsync ? 1 : 0);

        ImGui::Separator();
        ImGui::Text("Disable opaque geometry");
        ImGui::SameLine();
        static bool disableOpaque = false;
        ImGui::Checkbox("##opaque", &scene.disableOpaque);

        ImGui::Separator();

        for (auto* pass : test.pipeline.passes)
        {
            for (int i = 0; i < pass->attachments.size(); i += 3)
            {
                for (int j = 0; j < 3 && i+j < pass->attachments.size(); j++)
                {
                    RenderpassAttachment* attachment = pass->attachments[i + j];
                    ImGui::Image((void*)attachment->id, ImVec2(512, 512 * 9/16), ImVec2(0, 1), ImVec2(1, 0));
                    if (j < 2 && i+j < pass->attachments.size()-1)
                        ImGui::SameLine();
                }
                for (int j = 0; j < 3 && i+j < pass->attachments.size(); j++)
                {
                    RenderpassAttachment* attachment = pass->attachments[i + j];
                    char buf[128];
                    sprintf(buf, "%s (%d)", attachment->name, attachment->id);
                    ImGui::Text("%*s", j == 0 ? 0 : 80, buf);
                    if (j < 2 && i+j < pass->attachments.size()-1)
                        ImGui::SameLine();
                }
            }

            //for (auto* attachment : pass->attachments)
            //{
            //    ImGui::Image((void*)attachment->id, ImVec2(512, 512 * 9/16), ImVec2(0, 1), ImVec2(1, 0));
            //    ImGui::SameLine();
            //    ImGui::Text("%s (%d)", attachment->name, attachment->id);
            //}
        }
    }
    ImGui::End();
}

void PerfWidget(PerfData& perfData)
{
    ImGui::Text("Lifetime frametimes"); // TODO: center
    ImGui::Text("CPU Min: %.3fms Max: %.3fms Avg: %.3fms", perfData.cpu.lifetimeMinFrametime, perfData.cpu.lifetimeMaxFrametime, perfData.cpu.lifetimeAvgFrametime);
    ImGui::Text("GPU Min: %.3fms Max: %.3fms Avg: %.3fms", perfData.gpu.lifetimeMinFrametime, perfData.gpu.lifetimeMaxFrametime, perfData.gpu.lifetimeAvgFrametime);
    
    ImGui::Separator();
    ImGui::Text("Last 512 frametimes"); // TODO: center
    ImGui::Text("CPU Min: %.3fms Max: %.3fms Avg: %.3fms", perfData.cpu.minFrametime, perfData.cpu.maxFrametime, perfData.cpu.avgFrametime);
    ImGui::Text("GPU Min: %.3fms Max: %.3fms Avg: %.3fms", perfData.gpu.minFrametime, perfData.gpu.maxFrametime, perfData.gpu.avgFrametime);

    ImGui::PlotLines("CPU Frametime", perfData.cpu.data, perfData.cpu.filledBufferOnce ? PERF_DATAPOINT_COUNT : perfData.cpu.lifetimeDatapointCount, perfData.cpu.latestIndex, NULL, perfData.cpu.lifetimeMinFrametime, perfData.cpu.lifetimeMaxFrametime, ImVec2(0, 70.f));
    ImGui::PlotLines("GPU Frametime", perfData.gpu.data, perfData.gpu.filledBufferOnce ? PERF_DATAPOINT_COUNT : perfData.gpu.lifetimeDatapointCount, perfData.gpu.latestIndex, NULL, perfData.gpu.lifetimeMinFrametime, perfData.gpu.lifetimeMaxFrametime, ImVec2(0, 70.f));
}

void ShowPerfMetrics(DeferredTest& test)
{
    PerfWidget(test.pipeline.perfData);
    ImGui::Text("Frames: %d", test.pipeline.perfData.cpu.lifetimeDatapointCount);
    ImGui::Separator();
    ImGui::Separator();

    for (auto* renderpass : test.pipeline.passes)
    {
        if (ImGui::TreeNodeEx(renderpass->name, ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen))
        {
            PerfWidget(renderpass->perfData);

            for (auto* subpass : renderpass->subpasses)
            {
                if (ImGui::TreeNodeEx(subpass->name))
                {
                    PerfWidget(subpass->perfData);
                    ImGui::TreePop();
                }
            }
            ImGui::TreePop();
        }
    }
}

int main(void) 
{
    if (!glfwInit())
        return -1;

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

    glEnable (GL_DEBUG_OUTPUT);
    glDebugMessageCallback(MessageCallback, 0);

    ImGuiWrapper::Init(window);
    // NOTE: Fix my harware issue, for a phantom controller always holding down on one joystick
    ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad; 
    DeferredTest test;

    // Intro de-pixelation effect
    test.scene.sceneParams.pixelSize = 1000;
    test.scene.sceneParams.wireframe = 0;
    bool introDone = false;

    glClearColor(0.2f, 0.2f, 0.3f, 1.f);
    while (!glfwWindowShouldClose(window)) 
    {
        // Intro de-pixelation effect
        if (!introDone)
        {
            if (test.scene.sceneParams.pixelSize > 1)
                test.scene.sceneParams.pixelSize -= test.scene.sceneParams.pixelSize * 0.1;
            else
                introDone = true;
        }

        glfwPollEvents();
        glClear(GL_COLOR_BUFFER_BIT);

        ImGuiWrapper::PreRender();
        test.camera.Update(window);

        //ShowFPS(test);
        ShowSettings(test, test.scene);
        ShowPerfMetrics(test);

        test.Render();
        ImGuiWrapper::Render();

        glfwSwapBuffers(window);
    }

    ImGuiWrapper::Deinit();
    glfwTerminate();

    return 0;
}
