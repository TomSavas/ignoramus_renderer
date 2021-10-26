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
    Shader& dualDepthPeelingInitShader = shaders.AddShader("dual depth peeling init",
        ShaderDescriptor(
            {
                //ShaderDescriptor::File(SHADER_PATH "dual_depth_peeling_init.vert", ShaderDescriptor::VERTEX_SHADER),
                ShaderDescriptor::File(SHADER_PATH "default.vert", ShaderDescriptor::VERTEX_SHADER),
                ShaderDescriptor::File(SHADER_PATH "dual_depth_peeling_init.frag", ShaderDescriptor::FRAGMENT_SHADER),
            }));
    Shader& dualDepthPeelingShader = shaders.AddShader("dual depth peeling",
        ShaderDescriptor(
            {
                //ShaderDescriptor::File(SHADER_PATH "dual_depth_peeling.vert", ShaderDescriptor::VERTEX_SHADER),
                ShaderDescriptor::File(SHADER_PATH "default.vert", ShaderDescriptor::VERTEX_SHADER),
                ShaderDescriptor::File(SHADER_PATH "dual_depth_peeling.frag", ShaderDescriptor::FRAGMENT_SHADER),
            }));
    Shader& dualDepthPeelingBackBlendingShader = shaders.AddShader("dual depth peeling back blending",
        ShaderDescriptor(
            {
                //ShaderDescriptor::File(SHADER_PATH "dual_depth_peeling_back_blending.vert", ShaderDescriptor::VERTEX_SHADER),
                //ShaderDescriptor::File(SHADER_PATH "default.vert", ShaderDescriptor::VERTEX_SHADER),
                ShaderDescriptor::File(SHADER_PATH "texture.vert", ShaderDescriptor::VERTEX_SHADER),
                ShaderDescriptor::File(SHADER_PATH "dual_depth_peeling_back_blending.frag", ShaderDescriptor::FRAGMENT_SHADER),
            }));
    Shader& dualDepthPeelingCompositionShader = shaders.AddShader("dual depth peeling composition",
        ShaderDescriptor(
            {
                ShaderDescriptor::File(SHADER_PATH "texture.vert", ShaderDescriptor::VERTEX_SHADER),
                //ShaderDescriptor::File(SHADER_PATH "dual_depth_peeling_composition.vert", ShaderDescriptor::VERTEX_SHADER),
                ShaderDescriptor::File(SHADER_PATH "dual_depth_peeling_composition.frag", ShaderDescriptor::FRAGMENT_SHADER),
            }));

    scene.sceneParams.gamma = 1.5f; // sRGB = 2.2

    // TODO: potentially nicer architectural solution would be to redo everything into a "description"..?
    Renderpass& deferredLightingPass = pipeline.AddPass("Deferred lighting pass");
    RenderpassAttachment& deferredDepth    = deferredLightingPass.AddAttachment(RenderpassAttachment("g_depth",    AttachmentFormat::DEPTH));
    RenderpassAttachment& deferredPosition = deferredLightingPass.AddAttachment(RenderpassAttachment("g_position", AttachmentFormat::FLOAT_3));
    RenderpassAttachment& deferredNormal   = deferredLightingPass.AddAttachment(RenderpassAttachment("g_normal",   AttachmentFormat::FLOAT_3));
    RenderpassAttachment& deferredAlbedo   = deferredLightingPass.AddAttachment(RenderpassAttachment("g_albedo",   AttachmentFormat::FLOAT_3));
    RenderpassAttachment& deferredSpecular = deferredLightingPass.AddAttachment(RenderpassAttachment("g_specular", AttachmentFormat::FLOAT_3));
    deferredLightingPass.AddSubpass("Geometry subpass", &geometryShader, OPAQUE,
        {
            SubpassAttachment(&deferredDepth,    SubpassAttachment::AS_DEPTH),
            SubpassAttachment(&deferredPosition, SubpassAttachment::AS_COLOR),
            SubpassAttachment(&deferredNormal,   SubpassAttachment::AS_COLOR),
            SubpassAttachment(&deferredAlbedo,   SubpassAttachment::AS_COLOR),
            SubpassAttachment(&deferredSpecular, SubpassAttachment::AS_COLOR),
        });
    deferredLightingPass.AddSubpass("Composition-lighting subpass", &deferredLightingShader, SCREEN_QUAD,
        {
            // TODO: allow picking all existing renderpass' (specific subpass') attachments and re-binding them as textures with the same names?
            SubpassAttachment(&deferredLightingPass.AddOutputAttachment(), SubpassAttachment::AS_COLOR),
            SubpassAttachment(&deferredPosition, SubpassAttachment::AS_TEXTURE, "tex_pos"),
            SubpassAttachment(&deferredNormal,   SubpassAttachment::AS_TEXTURE, "tex_normal"),
            SubpassAttachment(&deferredAlbedo,   SubpassAttachment::AS_TEXTURE, "tex_diffuse"),
            SubpassAttachment(&deferredSpecular, SubpassAttachment::AS_TEXTURE, "tex_specular"),
        });

//#define DUAL_DEPTH_PEELING_TRANSPARENCY
#define DEPTH_PEELING_TRANSPARENCY
//#define UNSORTED_FORWARD_TRANSPARENCY

#ifdef DEPTH_PEELING_TRANSPARENCY
#define DEPTH_PASS_COUNT 6
    Renderpass& depthPeelingPass = pipeline.AddPass("Depth peeling pass");
    RenderpassAttachment& depthPeelingDepthA = depthPeelingPass.AddAttachment(RenderpassAttachment("depth_peel_depth_A", AttachmentFormat::DEPTH));
    RenderpassAttachment& depthPeelingDepthB = depthPeelingPass.AddAttachment(RenderpassAttachment("depth_peel_depth_B", AttachmentFormat::DEPTH));
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
                SubpassAttachment(&depthPeelingPass.AddAttachment(RenderpassAttachment(outputBuffer, AttachmentFormat::FLOAT_4)), SubpassAttachment::AS_COLOR), // Not actually necessary, just for debug purposes -- could blend into output immediatelly
                SubpassAttachment(evenPeel ? &depthPeelingDepthA : &depthPeelingDepthB,                     SubpassAttachment::AS_DEPTH),
                SubpassAttachment(evenPeel ? &depthPeelingDepthB : &depthPeelingDepthA,                     SubpassAttachment::AS_TEXTURE, "greater_depth"),
            }, peelSettings);
    }
#endif //DEPTH_PEELING_TRANSPARENCY

#ifdef DUAL_DEPTH_PEELING_TRANSPARENCY
#define DEPTH_PASS_COUNT 3
#define CLEAR_DEPTH -99999.f
#define MIN_DEPTH 0.f
#define MAX_DEPTH 1.f
    Renderpass& dualDepthPeelingPass = pipeline.AddPass("Dual depth peeling pass");
    RenderpassAttachment& minMaxDepthA = dualDepthPeelingPass.AddAttachment(RenderpassAttachment("min_max_depth_A", AttachmentFormat::FLOAT_2, AttachmentClearOpts(glm::vec4(CLEAR_DEPTH, CLEAR_DEPTH, 0.f, 0.f))));
    RenderpassAttachment& minMaxDepthB = dualDepthPeelingPass.AddAttachment(RenderpassAttachment("min_max_depth_B", AttachmentFormat::FLOAT_2, AttachmentClearOpts(glm::vec4(-MIN_DEPTH, MAX_DEPTH, 0.f, 0.f))));
    RenderpassAttachment& frontBlenderA = dualDepthPeelingPass.AddAttachment(RenderpassAttachment("front_blender_A", AttachmentFormat::FLOAT_4, AttachmentClearOpts(glm::vec4(0.f, 0.f, 0.f, 0.f))));
    RenderpassAttachment& frontBlenderB = dualDepthPeelingPass.AddAttachment(RenderpassAttachment("front_blender_B", AttachmentFormat::FLOAT_4, AttachmentClearOpts(glm::vec4(0.f, 0.f, 0.f, 0.f))));
    RenderpassAttachment& tempBackBlenderA = dualDepthPeelingPass.AddAttachment(RenderpassAttachment("temp_back_blender_A", AttachmentFormat::FLOAT_4, AttachmentClearOpts(glm::vec4(0.f, 0.f, 0.f, 0.f))));
    RenderpassAttachment& tempBackBlenderB = dualDepthPeelingPass.AddAttachment(RenderpassAttachment("temp_back_blender_B", AttachmentFormat::FLOAT_4, AttachmentClearOpts(glm::vec4(0.f, 0.f, 0.f, 0.f))));
    RenderpassAttachment& finalBackBlender = dualDepthPeelingPass.AddAttachment(RenderpassAttachment("back_blender", AttachmentFormat::FLOAT_4, AttachmentClearOpts(glm::vec4(0.5f, 0.5f, 0.5f, 0.f))));

    PassSettings dualDepthPeelingInitPassSettings = PassSettings::DefaultSubpassSettings();
    dualDepthPeelingInitPassSettings.ignoreApplication = false;
    dualDepthPeelingInitPassSettings.enable = { GL_BLEND };
    dualDepthPeelingInitPassSettings.blendEquation = GL_MAX;
    dualDepthPeelingPass.AddSubpass("Init subpass", &dualDepthPeelingInitShader, TRANSPARENT,
        {
            SubpassAttachment(&minMaxDepthA, SubpassAttachment::AS_COLOR)
        }, dualDepthPeelingInitPassSettings);

    // "First" pass is the initialization subpass
    for (int i = 1; i <= DEPTH_PASS_COUNT; i++)
    {
        PassSettings subpassSettings = PassSettings::DefaultSubpassSettings();
        subpassSettings.ignoreApplication = false;
        subpassSettings.enable = { GL_BLEND };
        subpassSettings.blendEquation = GL_MAX;

        bool evenPeel = i % 2 == 0;

        char* peelSubpassName = new char[64];
        sprintf(peelSubpassName, "Dual depth peel #%d subpass", i);
        Subpass& peelSubpass = dualDepthPeelingPass.AddSubpass(peelSubpassName, &dualDepthPeelingShader, TRANSPARENT,
            { 
                SubpassAttachment(evenPeel ? &minMaxDepthA     : &minMaxDepthB,     SubpassAttachment::AS_COLOR, AttachmentClearOpts(glm::vec4(CLEAR_DEPTH, CLEAR_DEPTH, 0.f, 0.f))),
                SubpassAttachment(evenPeel ? &frontBlenderA    : &frontBlenderB,    SubpassAttachment::AS_COLOR, AttachmentClearOpts(glm::vec4(0.f, 0.f, 0.f, 0.f))),
                SubpassAttachment(evenPeel ? &tempBackBlenderA : &tempBackBlenderB, SubpassAttachment::AS_COLOR, AttachmentClearOpts(glm::vec4(0.f, 0.f, 0.f, 0.f))),

                SubpassAttachment(evenPeel ? &minMaxDepthB     : &minMaxDepthA,     SubpassAttachment::AS_TEXTURE, "previousDepthBlender"),
                SubpassAttachment(evenPeel ? &frontBlenderB    : &frontBlenderA,    SubpassAttachment::AS_TEXTURE, "previousFrontBlender"),
            }, subpassSettings);

        char* blendingSubpassName = new char[64];
        sprintf(blendingSubpassName, "Dual depth peel #%d back blending subpass", i);
        subpassSettings = PassSettings::DefaultSubpassSettings();
        subpassSettings.ignoreApplication = false;
        subpassSettings.enable = { GL_BLEND };
        subpassSettings.blendEquation = GL_FUNC_ADD;
        subpassSettings.srcBlendFactor = GL_SRC_ALPHA;
        subpassSettings.dstBlendFactor = GL_ONE_MINUS_SRC_ALPHA;
        Subpass& backBlendSubpass = dualDepthPeelingPass.AddSubpass(blendingSubpassName, &dualDepthPeelingBackBlendingShader, SCREEN_QUAD,
            { 
                SubpassAttachment(&finalBackBlender, SubpassAttachment::AS_COLOR),
                SubpassAttachment(evenPeel ? &tempBackBlenderA : &tempBackBlenderB, SubpassAttachment::AS_TEXTURE, "tempBackBlender"),
            }, subpassSettings);
    }
#endif //DUAL_DEPTH_PEELING_TRANSPARENCY

    pipeline.AddOutputPass(quadTextureShader);

#ifdef UNSORTED_FORWARD_TRANSPARENCY
    PassSettings settings = PassSettings::DefaultOutputRenderpassSettings();
    settings.ignoreApplication = false;
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

#ifdef DUAL_DEPTH_PEELING_TRANSPARENCY
    PassSettings settings = PassSettings::DefaultOutputRenderpassSettings();
    settings.ignoreApplication = false;
    settings.ignoreClear = true;
    settings.enable = { GL_BLEND };
    settings.blendEquation = GL_FUNC_ADD;
    settings.srcBlendFactor = GL_SRC_ALPHA;
    settings.dstBlendFactor = GL_ONE_MINUS_SRC_ALPHA;

    Renderpass& dualDepthPeelingCompositionPass = pipeline.AddPass("Dual depth peeling composition pass", settings);
    dualDepthPeelingCompositionPass.fbo = 0;

    dualDepthPeelingCompositionPass.AddSubpass("Dual depth peeling composition subpass", &dualDepthPeelingCompositionShader, SCREEN_QUAD,
            {
                    // Default framebuffer already has a color attachment, no need to add another one
                SubpassAttachment(DEPTH_PASS_COUNT % 2 == 0 ? &frontBlenderA : &frontBlenderB, SubpassAttachment::AS_TEXTURE, "frontBlender"),
                SubpassAttachment(&finalBackBlender, SubpassAttachment::AS_TEXTURE, "backBlender"),
            }, settings);
#endif //DUAL_DEPTH_PEELING_TRANSPARENCY

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
            glDrawBuffers(renderpass.allColorAttachmentIndices.size(), renderpass.allColorAttachmentIndices.data());
        }
        renderpass.settings.Apply();
        renderpass.settings.Clear();
        previousSettings = renderpass.settings;

        // TODO: this duplicates clears and is generally pretty stupid
        {
            for (auto* attachment : renderpass.attachments)
            {
                if (attachment->hasSeparateClearOpts && attachment->attachmentIndex != INVALID_ATTACHMENT_INDEX)
                {
                    glDrawBuffers(1, &attachment->attachmentIndex);
                    glClearColor(attachment->clearOpts.color.x, attachment->clearOpts.color.y, attachment->clearOpts.color.z,
                            attachment->clearOpts.color.w);
                    glClear(GL_COLOR_BUFFER_BIT);
                }
            }
            if (renderpass.fbo != 0)
            {
                glDrawBuffers(renderpass.allColorAttachmentIndices.size(), renderpass.allColorAttachmentIndices.data());
            }
        }

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
            // TODO: this duplicates clears and is generally pretty stupid
            {
                for (auto& attachment : subpass.attachments)
                {
                    if (attachment.hasSeparateClearOpts && attachment.renderpassAttachment->attachmentIndex != INVALID_ATTACHMENT_INDEX)
                    {
                        glDrawBuffers(1, &attachment.renderpassAttachment->attachmentIndex);
                        glClearColor(attachment.clearOpts.color.x, attachment.clearOpts.color.y, attachment.clearOpts.color.z,
                                attachment.clearOpts.color.w);
                        glClear(GL_COLOR_BUFFER_BIT);
                    }
                }
                if (renderpass.fbo != 0)
                {
                    glDrawBuffers(subpass.colorAttachmentsToActivate.size(), subpass.colorAttachmentsToActivate.data());
                }
            }

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
