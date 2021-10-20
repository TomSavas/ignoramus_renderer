#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include <vector>
#include <algorithm>

#include "GLFW/glfw3.h"
#include "glm/gtx/transform.hpp"
#include "glm/gtx/string_cast.hpp"

#include "log.h"

#include "deferred_render_test.h"
#include "shader.h"
#include "scene.h"

#include "texture_pool.h"

static GLuint timeQuery;
DeferredTest::DeferredTest()
{
    Shader& quadTextureShader = shaders.AddShader("quad texture", 
        ShaderDescriptor(
            {
                ShaderDescriptor::File(SHADER_PATH "texture.vert", ShaderDescriptor::VERTEX_SHADER),
                ShaderDescriptor::File(SHADER_PATH "texture.frag", ShaderDescriptor::FRAGMENT_SHADER)
            }));
    Shader& geometryShader = shaders.AddShader("geometry",
        ShaderDescriptor(
            {
                ShaderDescriptor::File(SHADER_PATH "geometry_buffer.geom", ShaderDescriptor::GEOMETRY_SHADER),
                ShaderDescriptor::File(SHADER_PATH "geometry_buffer.vert", ShaderDescriptor::VERTEX_SHADER),
                ShaderDescriptor::File(SHADER_PATH "geometry_buffer.frag", ShaderDescriptor::FRAGMENT_SHADER)
            }));
    Shader& deferredLightingShader = shaders.AddShader("deferred lighting", 
        ShaderDescriptor(
            {
                ShaderDescriptor::File(SHADER_PATH "deferred_lighting.vert", ShaderDescriptor::VERTEX_SHADER),
                ShaderDescriptor::File(SHADER_PATH "deferred_lighting.frag", ShaderDescriptor::FRAGMENT_SHADER)
            }));
    Shader& forwardTransparencyShader = shaders.AddShader("forward transparency",
        ShaderDescriptor(
            {
                ShaderDescriptor::File(SHADER_PATH "forward_transparency.vert", ShaderDescriptor::VERTEX_SHADER),
                ShaderDescriptor::File(SHADER_PATH "forward_transparency.frag", ShaderDescriptor::FRAGMENT_SHADER)
            }));
    Shader& depthPeelingShader = shaders.AddShader("depth peeling", 
        ShaderDescriptor(
            {
                ShaderDescriptor::File(SHADER_PATH "depth_peeling.vert", ShaderDescriptor::VERTEX_SHADER),
                ShaderDescriptor::File(SHADER_PATH "depth_peeling.frag", ShaderDescriptor::FRAGMENT_SHADER)
            }));

    scene.sceneParams.gamma = 1.5f; // sRGB = 2.2

    // TODO: potentially nicer architectural solution would be to redo everything into a "description"..?
    Renderpass& deferredLightingPass = pipeline.AddPass("Deferred lighting pass");
    deferredLightingPass.AddSubpass("Geometry subpass", &geometryShader, OPAQUE,
        {
#define G_BUF_DEPTH    "g_depth"
            SubpassAttachment(&deferredLightingPass.AddAttachment(G_BUF_DEPTH,    AttachmentFormat::DEPTH),   SubpassAttachment::AS_DEPTH),
#define G_BUF_POSITION "g_position"
            SubpassAttachment(&deferredLightingPass.AddAttachment(G_BUF_POSITION, AttachmentFormat::FLOAT_3), SubpassAttachment::AS_COLOR),
#define G_BUF_NORMAL   "g_normal"
            SubpassAttachment(&deferredLightingPass.AddAttachment(G_BUF_NORMAL,   AttachmentFormat::FLOAT_3), SubpassAttachment::AS_COLOR),
#define G_BUF_ALBEDO   "g_albedo"
            SubpassAttachment(&deferredLightingPass.AddAttachment(G_BUF_ALBEDO,   AttachmentFormat::FLOAT_3), SubpassAttachment::AS_COLOR),
#define G_BUF_SPECULAR "g_specular"
            SubpassAttachment(&deferredLightingPass.AddAttachment(G_BUF_SPECULAR, AttachmentFormat::FLOAT_3), SubpassAttachment::AS_COLOR),
        });
    deferredLightingPass.AddSubpass("Composition-lighting subpass", &deferredLightingShader, SCREEN_QUAD,
        {
            // TODO: allow picking all existing renderpass' (specific subpass') attachments and re-binding them as textures with the same names?
            SubpassAttachment(&deferredLightingPass.AddOutputAttachment(),         SubpassAttachment::AS_COLOR),
            SubpassAttachment(&deferredLightingPass.GetAttachment(G_BUF_ALBEDO),   SubpassAttachment::AS_TEXTURE, "tex_diffuse"),
            SubpassAttachment(&deferredLightingPass.GetAttachment(G_BUF_POSITION), SubpassAttachment::AS_TEXTURE, "tex_pos"),
            SubpassAttachment(&deferredLightingPass.GetAttachment(G_BUF_NORMAL),   SubpassAttachment::AS_TEXTURE, "tex_normal"),
            SubpassAttachment(&deferredLightingPass.GetAttachment(G_BUF_SPECULAR), SubpassAttachment::AS_TEXTURE, "tex_specular"),
        });

//#define DEPTH_PEELING_TRANSPARENCY
#define UNSORTED_FORWARD_TRANSPARENCY

#ifdef DEPTH_PEELING_TRANSPARENCY
#define DEPTH_PASS_COUNT 6
    Renderpass& depthPeelingPass = pipeline.AddPass("Depth peeling pass");
    RenderpassAttachment& depthPeelingDepthA = depthPeelingPass.AddAttachment("depth_peel_depth_A", AttachmentFormat::DEPTH);
    RenderpassAttachment& depthPeelingDepthB = depthPeelingPass.AddAttachment("depth_peel_depth_B", AttachmentFormat::DEPTH);
    for (int i = 0; i < DEPTH_PASS_COUNT; i++)
    {
        char* subpassName = new char[64];
        sprintf(subpassName, "Depth peel #%d subpass", i);
        char* outputBuffer = new char[64];
        sprintf(outputBuffer, "depth_peel_%d_output", i);

        PassSettings peelSettings = PassSettings::DefaultSettings();
        peelSettings.enable = { GL_DEPTH_TEST };
        peelSettings.clearColor = glm::vec4(0.f, 0.f, 0.f, 0.f);

        bool evenPeel = i % 2 == 0;
        Subpass& subpass = depthPeelingPass.AddSubpass(subpassName, &depthPeelingShader, TRANSPARENT,
            { 
                SubpassAttachment(&depthPeelingPass.AddAttachment(outputBuffer, AttachmentFormat::FLOAT_4), SubpassAttachment::AS_COLOR), // Not actually necessary, just for debug purposes
                SubpassAttachment(evenPeel ? &depthPeelingDepthA : &depthPeelingDepthB,                     SubpassAttachment::AS_DEPTH),
                SubpassAttachment(evenPeel ? &depthPeelingDepthB : &depthPeelingDepthA,                     SubpassAttachment::AS_TEXTURE, "greater_depth"),
            }, peelSettings);
    }
#endif //DEPTH_PEELING_TRANSPARENCY

#ifdef DUAL_DEPTH_PEELING_TRANSPARENCY
#define DEPTH_PASS_COUNT 6
#endif //DUAL_DEPTH_PEELING_TRANSPARENCY

    pipeline.AddOutputPass(quadTextureShader);

#ifdef UNSORTED_FORWARD_TRANSPARENCY
    PassSettings settings = PassSettings::DefaultOutputRenderpassSettings();
    settings.ignoreClear = true;
    settings.enable = { GL_BLEND, };
    //settings.enable.push_back(GL_DEPTH_TEST);
    settings.srcBlendFactor = GL_SRC_ALPHA;
    settings.dstBlendFactor = GL_ONE_MINUS_SRC_ALPHA;
    Renderpass& forwardTransparencyRenderpass = pipeline.AddPass("Forward transparency pass", settings);
    forwardTransparencyRenderpass.fbo = 0;

    //Subpass& subpass = forwardTransparencyRenderpass.AddSubpass("Forward transparency subass",
    //        new Shader("../src/shaders/forward_transparency.vert", "../src/shaders/forward_transparency.frag"), TRANSPARENT, {}, settings);

    Subpass& subpass = forwardTransparencyRenderpass.AddSubpass("Forward transparency subass", &forwardTransparencyShader, TRANSPARENT, 
            {
                //SubpassAttachment(&deferredLightingPass.GetAttachment(G_BUF_DEPTH),   SubpassAttachment::AS_DEPTH),
                //SubpassAttachment(&forwardTransparencyRenderpass.AddAttachment("output", AttachmentFormat::FLOAT_4), SubpassAttachment::AS_COLOR),
                //SubpassAttachment(&forwardTransparencyRenderpass.AddAttachment("depth",  AttachmentFormat::DEPTH),   SubpassAttachment::AS_DEPTH),
            }, settings);
#endif //UNSORTED_FORWARD_TRANSPARENCY


#ifdef MESHKIN_OIT_FORWARD_TRANSPARENCY
#endif //MESHKIN_OIT_FORWARD_TRANSPARENCY

#ifdef BAVOIL_MYERS_OIT_FORWARD_TRANSPARENCY
#endif //BAVOIL_MYERS_OIT_FORWARD_TRANSPARENCY

#ifdef WEIGHTED_BLENDED_OIT_FORWARD_TRANSPARENCY
#endif //WEIGHTED_BLENDED_OIT_FORWARD_TRANSPARENCY

#ifdef DEPTH_PEELING_TRANSPARENCY
    // TEMP, compositing depth peels on top of our output. Should be done using a stencil. Would save on a lot of useless black drawing
    {
        for (int i = DEPTH_PASS_COUNT-1; i >= 0; i--)
        {
            char* name = new char[64];
            sprintf(name, "output%d", i);
            char* subpassName = new char[64];
            sprintf(subpassName, "output%dsubpass", i);
            char* layer = new char[64];
            sprintf(layer, "depth_peel_%d_output", i);

            PassSettings settings = PassSettings::DefaultOutputRenderpassSettings();
            settings.ignoreClear = true;
            settings.enable = { GL_BLEND };
            settings.srcBlendFactor = GL_SRC_ALPHA;
            settings.dstBlendFactor = GL_ONE_MINUS_SRC_ALPHA;
            Renderpass& outputPass = pipeline.AddPass(name, settings);
            outputPass.fbo = 0;

            Subpass& subpass = outputPass.AddSubpass(subpassName, &quadTextureShader, SCREEN_QUAD,
                {
                    // Default framebuffer already has a color attachment, no need to add another one
                    SubpassAttachment(&depthPeelingPass.GetAttachment(layer), SubpassAttachment::AS_TEXTURE, "tex")
                }, settings);
        }
    }
#endif //DEPTH_PEELING_TRANSPARENCY

    assert(pipeline.ConfigureAttachments());

    Model transparentModel = Model("../assets/dragon.obj");
    Model model = Model("../assets/sponza/sponza.obj");
    Material* opaqueMat = new OpaqueMaterial();
    for (auto& mesh : model.meshes)
    {
        mesh.transform = Transform(glm::vec3(0.f, -10.f, 0.f), glm::quat(glm::vec3(0.f, 0.f, 0.f)), glm::vec3(5.f, 5.f, 5.f));
        scene.meshes[mesh.meshTag].push_back({ mesh, opaqueMat });
    }

    TransparentMaterial* transparentMat = new TransparentMaterial(0.f, 0.f, 1.f, 0.2f);
    transparentMat->Bind();
    transparentMat->UpdateData();
    for (auto& mesh : transparentModel.meshes)
    {
        mesh.transform = Transform(glm::vec3(0.f, 200.f, 0.f), glm::quat(glm::vec3(0.f, 0.f, 0.f)), glm::vec3(750.f, 750.f, 750.f));
        scene.meshes[TRANSPARENT].push_back({ mesh, transparentMat });
    }
    transparentMat = new TransparentMaterial(0.f, 1.f, 0.f, 0.4f);
    transparentMat->Bind();
    transparentMat->UpdateData();
    for (auto& mesh : transparentModel.meshes)
    {
        mesh.transform = Transform(glm::vec3(500.f, 200.f, 0.f), glm::quat(glm::vec3(0.f, 0.f, 0.f)), glm::vec3(750.f, 750.f, 750.f));
        scene.meshes[TRANSPARENT].push_back({ mesh, transparentMat });
    }
    transparentMat = new TransparentMaterial(1.f, 0.f, 0.f, 0.7f);
    transparentMat->Bind();
    transparentMat->UpdateData();
    for (auto& mesh : transparentModel.meshes)
    {
        mesh.transform = Transform(glm::vec3(-500.f, 200.f, 0.f), glm::quat(glm::vec3(0.f, 0.f, 0.f)), glm::vec3(750.f, 750.f, 750.f));
        scene.meshes[TRANSPARENT].push_back({ mesh, transparentMat });
    }

    scene.meshes[SCREEN_QUAD].push_back({ Mesh::ScreenQuadMesh(), new EmptyMaterial() });

    // TODO: potentially have more ubos for model params, so that we're not making unnecessary data uploads to the gpu
    glGenBuffers(1, &ubo);
    glBindBufferBase(GL_UNIFORM_BUFFER, Shader::modelParamBindingPoint, ubo);

    glGenQueries(1, &timeQuery);
}

void DeferredTest::Render()
{
    shaders.ReloadChangedShaders();

    // A la syncing with the game thread
    scene.mainCameraParams.pos = camera.transform.pos;
    scene.mainCameraParams.view = camera.View();
    scene.mainCameraParams.projection = camera.projection;

    // TODO: don't really need to do every frame
    scene.BindSceneParams();
    // TODO: support for multiple cameras
    scene.BindCameraParams();
    scene.BindLighting();

    float pipelineCpuDurationMs = 0.f;
    float pipelineGpuDurationMs = 0.f;

    static PassSettings previousSettings = PassSettings::DefaultSettings();
    for (int i = 0; i < pipeline.passes.size(); i++)
    {
        int activatedTextureCount = 0;
        ASSERT(pipeline.passes[i] != nullptr);
        Renderpass& renderpass = *pipeline.passes[i];

        float renderpassCpuDurationMs = 0.f;
        float renderpassGpuDurationMs = 0.f;

        glBindFramebuffer(GL_FRAMEBUFFER, renderpass.fbo);

        // TODO: srsly?
        for (int j = 0; j < previousSettings.enable.size(); j++)
        {
            bool disable = true;
            for (int k = 0; k < renderpass.settings.enable.size(); k++)
            {
                if (previousSettings.enable[j] == renderpass.settings.enable[k])
                {
                    disable = false;
                    break;
                }
            }

            if (disable)
            {
                glDisable(previousSettings.enable[j]);
            }
        }
        if (renderpass.fbo != 0)
        {
            glDrawBuffers(renderpass.colorAttachmentIndices.size(), renderpass.colorAttachmentIndices.data());
        }
        renderpass.settings.Apply();
        renderpass.settings.Clear();
        previousSettings = renderpass.settings;

        for (int j = 0; j < renderpass.subpasses.size(); j++)
        {
            ASSERT(renderpass.subpasses[j] != nullptr);
            Subpass& subpass = *renderpass.subpasses[j];

            // Maybe move this after all the clears?
            if (subpass.acceptedMeshTags == OPAQUE && scene.disableOpaque)
            {
                continue;
            }

            clock_t subpassStartTime = clock();
            glBeginQuery(GL_TIME_ELAPSED, timeQuery);

            subpass.shader->Use();

            for (SubpassAttachment& subpassAttachment : subpass.attachments)
            {
                ASSERT(subpassAttachment.renderpassAttachment != nullptr);

                unsigned int attachmentId = subpassAttachment.renderpassAttachment->id;
                switch (subpassAttachment.type)
                {
                    case SubpassAttachment::AS_COLOR:
                        continue;
                    case SubpassAttachment::AS_DEPTH:
                        // Remember, for the time being everything's a texture!
                        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, attachmentId, 0);
                        break;
                    case SubpassAttachment::AS_TEXTURE:
                        // TODO: Absolutely idiotic this. Should have a MRU eviction policy cache for texture units
                        // ... maybe MRU is not the best choice? No clue, needs testing
                        glActiveTexture(GL_TEXTURE0 + activatedTextureCount);
                        subpass.shader->SetUniform(subpassAttachment.useFor, activatedTextureCount++);
                        glBindTexture(GL_TEXTURE_2D, attachmentId);
                        break;
                    case SubpassAttachment::AS_BLIT:
                        ASSERT(false);
                        break;
                }

                if (strcmp(renderpass.name, "Depth peeling pass") == 0 && j == 0)
                {
                    float clearValue = 0.f;
                    glClearTexImage(attachmentId, 0, GL_DEPTH_COMPONENT, GL_FLOAT, &clearValue);
                }
            }

            // TODO: seriously?
            for (int k = 0; k < previousSettings.enable.size(); k++)
            {
                bool disable = true;
                for (int l = 0; l < subpass.settings.enable.size(); l++)
                {
                    if (previousSettings.enable[k] == subpass.settings.enable[l])
                    {
                        disable = false;
                        break;
                    }
                }

                if (disable)
                {
                    glDisable(previousSettings.enable[k]);
                }
            }
            if (renderpass.fbo != 0)
            {
                glDrawBuffers(subpass.colorAttachmentsToActivate.size(), subpass.colorAttachmentsToActivate.data());
            }
            subpass.settings.Apply();
            subpass.settings.Clear();
            previousSettings = subpass.settings;

            subpass.shader->AddDummyForUnboundTextures(pipeline.dummyTextureUnit);
            for (MeshWithMaterial& meshWithMaterial : scene.meshes[subpass.acceptedMeshTags])
            {
                // TODO: not necessary if already bound
                //       even if bound and changed can only partially update
                meshWithMaterial.material->Bind();

                glm::mat4 model = meshWithMaterial.mesh.transform.Model();
                glBindBuffer(GL_UNIFORM_BUFFER, ubo);
                glBufferData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), &model, GL_STATIC_DRAW);
                //glBindBuffer(GL_UNIFORM_BUFFER, 0);

                // TODO: Should be reworked - there should be some sort of communication between the shader and model here.
                // Shader dictates what textures it needs, the model provides them
                meshWithMaterial.mesh.Render(*subpass.shader);
            }

            clock_t subpassEndTime = clock();
            clock_t subpassCpuDuration = subpassEndTime - subpassStartTime;
            float subpassCpuDurationMs = subpassCpuDuration * 1000.f / CLOCKS_PER_SEC;
            subpass.perfData.cpu.AddFrametime(subpassCpuDurationMs);
            renderpassCpuDurationMs += subpassCpuDurationMs;

            glEndQuery(GL_TIME_ELAPSED);
            bool queryDone = false;
            while (!queryDone) 
            {
                glGetQueryObjectiv(timeQuery, GL_QUERY_RESULT_AVAILABLE, (int*)&queryDone);
            }
            long subpassGpuDurationNs;
            glGetQueryObjecti64v(timeQuery, GL_QUERY_RESULT, &subpassGpuDurationNs);
            double subpassGpuDurationMs = (double)subpassGpuDurationNs / 1000000.0;
            renderpassGpuDurationMs += subpassGpuDurationMs;
            subpass.perfData.gpu.AddFrametime(subpassGpuDurationMs);
        }
        renderpass.perfData.cpu.AddFrametime(renderpassCpuDurationMs);
        renderpass.perfData.gpu.AddFrametime(renderpassGpuDurationMs);

        pipelineCpuDurationMs += renderpassCpuDurationMs;
        pipelineGpuDurationMs += renderpassGpuDurationMs;
    }
    pipeline.perfData.cpu.AddFrametime(pipelineCpuDurationMs);
    pipeline.perfData.gpu.AddFrametime(pipelineGpuDurationMs);
}
