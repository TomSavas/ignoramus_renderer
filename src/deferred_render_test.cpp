#include <stdio.h>
#include <string.h>
#include <assert.h>

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

DeferredTest::DeferredTest()
{
    scene.sceneParams.gamma = 1.5f; // sRGB = 2.2

    // TODO: potentially nicer architectural solution would be to redo everything into a "description"..?
    Renderpass& deferredLightingPass = pipeline.AddPass("Deferred lighting pass");
    deferredLightingPass.AddSubpass("Geometry subpass", new Shader("../src/shaders/g_buf.vert", "../src/shaders/g_buf.geom", "../src/shaders/g_buf.frag"), OPAQUE,
        {
#define G_BUF_DEPTH    "g_depth"
            { &deferredLightingPass.AddAttachment(G_BUF_DEPTH,    AttachmentFormat::DEPTH),   SubpassAttachment::AS_DEPTH },
#define G_BUF_POSITION "g_position"
            { &deferredLightingPass.AddAttachment(G_BUF_POSITION, AttachmentFormat::FLOAT_3), SubpassAttachment::AS_COLOR },
#define G_BUF_NORMAL   "g_normal"
            { &deferredLightingPass.AddAttachment(G_BUF_NORMAL,   AttachmentFormat::FLOAT_3), SubpassAttachment::AS_COLOR },
#define G_BUF_ALBEDO   "g_albedo"
            { &deferredLightingPass.AddAttachment(G_BUF_ALBEDO,   AttachmentFormat::FLOAT_3), SubpassAttachment::AS_COLOR },
#define G_BUF_SPECULAR "g_specular"
            { &deferredLightingPass.AddAttachment(G_BUF_SPECULAR, AttachmentFormat::FLOAT_3), SubpassAttachment::AS_COLOR },
        });
    deferredLightingPass.AddSubpass("Composition-lighting subpass", new Shader("../src/shaders/deferred_lighting.vert", "../src/shaders/deferred_lighting.frag"), SCREEN_QUAD,
        {
            // TODO: allow picking all existing renderpass' (specific subpass') attachments and re-binding them as textures with the same names?
            SubpassAttachment(&deferredLightingPass.AddOutputAttachment(),         SubpassAttachment::AS_COLOR),
            SubpassAttachment(&deferredLightingPass.GetAttachment(G_BUF_ALBEDO),   SubpassAttachment::AS_TEXTURE, "tex_diffuse"),
            SubpassAttachment(&deferredLightingPass.GetAttachment(G_BUF_POSITION), SubpassAttachment::AS_TEXTURE, "tex_pos"),
            SubpassAttachment(&deferredLightingPass.GetAttachment(G_BUF_NORMAL),   SubpassAttachment::AS_TEXTURE, "tex_normal"),
            SubpassAttachment(&deferredLightingPass.GetAttachment(G_BUF_SPECULAR), SubpassAttachment::AS_TEXTURE, "tex_specular"),
        });

    // TODO
    //Renderpass& forwardTransparencyPass;

    Renderpass& depthPeelingPass = pipeline.AddPass("Depth peeling pass");
    RenderpassAttachment& depthPeelingDepthA = depthPeelingPass.AddAttachment("depth_peel_depth_A", AttachmentFormat::DEPTH);
    RenderpassAttachment& depthPeelingDepthB = depthPeelingPass.AddAttachment("depth_peel_depth_B", AttachmentFormat::DEPTH);
    //depthPeelingPass.RequestAttachmentFromPipeline("g_depth", SubpassAttachment::AS_BLIT, "depth_peel_depth_B"); // TODO
#define DEPTH_PASS_COUNT 6
    for (int i = 0; i < DEPTH_PASS_COUNT; i++)
    {
        char* subpassName = new char[64];
        sprintf(subpassName, "Depth peel #%d subpass", i);
        char* outputBuffer = new char[64];
        sprintf(outputBuffer, "depth_peel_%d_output", i);

        PassSettings peelSettings = PassSettings::DefaultSettings();
        peelSettings.enable = { GL_DEPTH_TEST };
        //peelSettings.enable = { GL_DEPTH_TEST, GL_CULL_FACE };
        peelSettings.clearColor = glm::vec4(0.f, 0.f, 0.f, 0.f);

        bool evenPeel = i % 2 == 0;
        Subpass& subpass = depthPeelingPass.AddSubpass(subpassName, new Shader("../src/shaders/depth_peeling.vert", "../src/shaders/depth_peeling.frag"), TRANSPARENT,
            { 
                SubpassAttachment(&depthPeelingPass.AddAttachment(outputBuffer, AttachmentFormat::FLOAT_4), SubpassAttachment::AS_COLOR),
                SubpassAttachment(evenPeel ? &depthPeelingDepthA : &depthPeelingDepthB,                     SubpassAttachment::AS_DEPTH),
                SubpassAttachment(evenPeel ? &depthPeelingDepthB : &depthPeelingDepthA,                     SubpassAttachment::AS_TEXTURE, "greater_depth"),
            }, peelSettings);
    }

    // TODO: factor out adding a dummy pass
    //pipeline
    //    .AddPass("Debug draw pass")
    //    .AddSubpass("empty", new Shader("../src/shaders/texture.vert", "../src/shaders/texture.frag"), NONE, { SubpassAttachment(&pipeline.DummyAttachment(), SubpassAttachment::AS_COLOR) });

    //Renderpass& postProFxPass = pipeline.AddPass("Post processing fx pass");
    //postProFxPass.AddSubpass("pixelation", new Shader("../src/shaders/texture.vert", "../src/shaders/pixelation.frag"), SCREEN_QUAD,
    //    { 
    //        SubpassAttachment(&postProFxPass.AddOutputAttachment(), SubpassAttachment::AS_COLOR),
    //        // TEMP, should have "GetPreviousOutput" or something
    //        SubpassAttachment(&deferredLightingPass.AddOutputAttachment(), SubpassAttachment::AS_TEXTURE, "tex") 
    //    });

    pipeline.AddOutputPass();
    // TEMP, compositing depth peels on top of our output. Should be done using a stencil. Would save on a lot of useless black drawing
    {
        Shader* texShader = new Shader("../src/shaders/texture.vert", "../src/shaders/texture.frag");
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

            Subpass& subpass = outputPass.AddSubpass(subpassName, texShader, SCREEN_QUAD,
                {
                    // Default framebuffer already has a color attachment, no need to add another one
                    SubpassAttachment(&depthPeelingPass.GetAttachment(layer), SubpassAttachment::AS_TEXTURE, "tex")
                }, settings);
        }
    }

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
    LOG_DEBUG("transparent mat size: %d", transparentMat->nonResourceData.size);
    LOG_ERROR("Transparent mat is messed up when using vec3 for color!");

    scene.meshes[SCREEN_QUAD].push_back({ Mesh::ScreenQuadMesh(), new EmptyMaterial() });

    // TODO: potentially have more ubos for model params, so that we're not making unnecessary data uploads to the gpu
    glGenBuffers(1, &ubo);
    glBindBufferBase(GL_UNIFORM_BUFFER, Shader::modelParamBindingPoint, ubo);
}

void DeferredTest::Render()
{
    // A la syncing with the game thread
    scene.mainCameraParams.pos = camera.transform.pos;
    scene.mainCameraParams.view = camera.View();
    scene.mainCameraParams.projection = camera.projection;

    // TODO: don't really need to do every frame
    scene.BindSceneParams();
    // TODO: support for multiple cameras
    scene.BindCameraParams();
    scene.BindLighting();

    static PassSettings previousSettings = PassSettings::DefaultSettings();
    for (int i = 0; i < pipeline.passes.size(); i++)
    {
        int activatedTextureCount = 0;
        ASSERT(pipeline.passes[i] != nullptr);
        Renderpass& renderpass = *pipeline.passes[i];

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
        }
    }
}
