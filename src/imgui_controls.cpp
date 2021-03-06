#include <vector>

#define GLX_GLXEXT_PROTOTYPES
#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include "GLFW/glfw3.h"

#include "imgui.h"

#include "render_pipeline.h"
#include "scene.h"
#include "test_structures.h"

void ShowInfo(Scene& scene, NamedPipeline& pipeline, bool* open)
{
    const float PAD = 10.0f;

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 work_pos = viewport->WorkPos; // Use work area to avoid menu-bar/task-bar, if any!
    ImVec2 work_size = viewport->WorkSize;
    ImVec2 window_pos;
    window_pos.x = work_pos.x + work_size.x - PAD;
    window_pos.y = work_pos.y + PAD;
    ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, ImVec2(1.f, 0.f));

    ImGui::SetNextWindowBgAlpha(0.35f);
    if (ImGui::Begin("Info", open, window_flags))
    {
        ImGui::Text("Ignoramus renderer");
        ImGui::SameLine();

#ifdef DEBUG
        ImGui::TextColored(ImVec4(1.f, 0.f, 0.f, 1.f), "DEBUG");
#else
        ImGui::TextColored(ImVec4(0.f, 1.f, 0.f, 1.f), "RELEASE");
#endif
        ImGui::Separator();

        static const unsigned char *renderer = glGetString(GL_RENDERER);
        static const unsigned char *version = glGetString(GL_VERSION);
        ImGui::Text("%s\n%s", renderer, version);
        ImGui::Separator();

        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::SameLine();
        ImGui::Text("Frametime: %.2fms", ImGui::GetIO().DeltaTime * 1000);
        ImGui::Separator();

        ImGui::Text("Pipeline: %s", pipeline.name);
        ImGui::Text("Camera pos: %f %f %f", scene.camera.transform.pos.x, scene.camera.transform.pos.y, scene.camera.transform.pos.z);
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

void ShowPerfMetrics(RenderPipeline& pipeline, bool* open)
{
    if (ImGui::Begin("Perf metrics", open))
    {
        PerfWidget(pipeline.perfData);
        ImGui::Text("Frames: %d", pipeline.perfData.cpu.lifetimeDatapointCount);
        ImGui::Separator();
        ImGui::Separator();

        for (auto* renderpass : pipeline.passes)
        {
            if (ImGui::TreeNodeEx(renderpass->name, ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen))
            {
                PerfWidget(renderpass->perfData);

                char subpassLabel[128];
                for (auto* subpass : renderpass->subpasses)
                {
                    sprintf(subpassLabel, "%s (CPU: %.3fms, GPU: %.3fms)", subpass->name, subpass->perfData.cpu.avgFrametime, subpass->perfData.gpu.avgFrametime);
                    if (ImGui::TreeNodeEx(subpassLabel))
                    {
                        PerfWidget(subpass->perfData);
                        ImGui::TreePop();
                    }
                }
                ImGui::TreePop();
            }
        }
    }
    ImGui::End();
}

void ShowPipelineResources(NamedPipeline& namedPipeline, bool* open)
{
    if (ImGui::Begin("Pipeline resources", open))
    {
        static int imagesPerLine = 3;

        ImGui::SliderInt("Images per line", &imagesPerLine, 1, 10);
        int maxWidthPixels = ImGui::GetWindowContentRegionMax().x;
        int imageWidth = maxWidthPixels / (float)imagesPerLine;

        for (auto* pass : namedPipeline.pipeline.passes)
        {
            if (!ImGui::TreeNodeEx(pass->name, ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen))
            {
                continue;
            }

            std::vector<RenderpassAttachment*> attachments;
            for (int i = 0; i < pass->attachments.size(); i++)
            {
                RenderpassAttachment* attachment = pass->attachments[i];

                if (attachment->format != AttachmentFormat::SSBO && attachment->format != AttachmentFormat::ATOMIC_COUNTER)
                {
                    attachments.push_back(attachment);
                }
            }

            for (int i = 0; i < attachments.size(); i += imagesPerLine)
            {
                for (int j = 0; j < imagesPerLine && i+j < attachments.size(); j++)
                {
                    RenderpassAttachment* attachment = attachments[i + j];

                    ImGui::Image((void*)attachment->id, ImVec2(imageWidth, imageWidth * 9/16), ImVec2(0, 1), ImVec2(1, 0));
                    if (j < imagesPerLine-1 && i+j < attachments.size()-1)
                        ImGui::SameLine();
                }

                for (int j = 0; j < imagesPerLine && i+j < attachments.size(); j++)
                {
                    RenderpassAttachment* attachment = attachments[i + j];

                    char buf[128];
                    sprintf(buf, "%s (%d)", attachment->name, attachment->id);
                    ImVec2 textWidth = ImGui::CalcTextSize(buf);

                    ImGui::SetCursorPosX(imageWidth * j + int(textWidth.x / 2));
                    ImGui::Text("%s", buf);
                    if (j < imagesPerLine-1 && i+j < attachments.size()-1)
                        ImGui::SameLine();
                }
            }

            ImGui::TreePop();
        }
    }

    ImGui::End();
}

void ShowSceneSettings(Scene& scene, bool* open)
{
    if (ImGui::Begin("Scene settings", open))
    {
        ImGui::Checkbox("Wireframe", (bool*)&scene.sceneParams.wireframe);
        ImGui::Checkbox("Use depth from PPLL for tile culling", (bool*)&scene.sceneParams.useDepthLightCullingOptimisation);
        ImGui::Checkbox("Particles", (bool*)&scene.renderParticles);

        ImGui::SliderFloat("Gamma", &scene.sceneParams.gamma, 1, 10);
        ImGui::SliderFloat("SpecularPower", &scene.sceneParams.specularPower, 1, 1000);

        ImGui::SliderFloat("Directional light shadow bias", &scene.lighting.directionalBiasAndAngleBias.x, 0.000001, 0.1, "%.6f");
        ImGui::SliderFloat("Directional light shadow angle bias", &scene.lighting.directionalBiasAndAngleBias.y, 0.000001, 0.1, "%.6f");
    }
    ImGui::End();
}


void ShowControls(GLFWwindow* window, std::vector<NamedPipeline>& pipelines, int& activePipelineIndex, Scene& scene, ShaderPool& shaders)
{
    static bool showMainMenuBar = false;
    static int lastMainMenuToggleButtonState = GLFW_RELEASE;
    int mainMenuToggleButtonState = glfwGetKey(window, GLFW_KEY_LEFT_ALT);
    if (mainMenuToggleButtonState != lastMainMenuToggleButtonState && mainMenuToggleButtonState == GLFW_PRESS)
    {
        showMainMenuBar = !showMainMenuBar;
    }
    lastMainMenuToggleButtonState = mainMenuToggleButtonState;

    static bool showPerfMetrics = false;
    static bool showPipelineSettings = false;
    static bool showPipelineResources = false;
    static bool showSceneSettings = false;
    static bool showInfo = true; 
    if (showMainMenuBar && ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("Pipeline selector"))
        {
            for (int i = 0; i < pipelines.size(); i++)
            {
                if (ImGui::RadioButton(pipelines[i].name, &activePipelineIndex, i))
                {
                    scene.sceneParams.useDepthLightCullingOptimisation = strstr(pipelines[i].name, "PPLL") != NULL ? 1 : 0;
                }
            }
            ImGui::EndMenu();
        }

        ImGui::Checkbox("Perf metrics", &showPerfMetrics);
        ImGui::Checkbox("Pipeline resources", &showPipelineResources);
        ImGui::Checkbox("Pipeline settings", &showPipelineSettings);
        ImGui::Checkbox("Scene settings", &showSceneSettings);
        ImGui::Checkbox("Info", &showInfo);

        ImGui::EndMainMenuBar();
    }

    if (showPerfMetrics)
    {
        ShowPerfMetrics(pipelines[activePipelineIndex].pipeline, &showPerfMetrics);
    }

    if (showPipelineResources)
    {
        ShowPipelineResources(pipelines[activePipelineIndex], &showPipelineResources);
    }

    if (showSceneSettings)
    {
        ShowSceneSettings(scene, &showSceneSettings);
    }

    if (showInfo)
    {
        ShowInfo(scene, pipelines[activePipelineIndex], &showInfo);
    }
}
