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

#ifdef DEBUG
    #define GL_ERROR_GUARD(msg) do {                          \
            GLenum err = glGetError();                        \
            if (err != GL_NO_ERROR)                           \
            {                                                 \
                printf("OpenGL error: %04x. %s\n", err, msg); \
                assert(false);                                \
            }                                                 \
        } while(0);
#else
    #define GL_ERROR_GUARD(msg) ;
#endif


DeferredTest::DeferredTest()
{
    scene.sceneParams.gamma = 1.5f; // sRGB = 2.2

    /*
    Renderpass directionalShadowMapPass = Renderpass("directional shadow pass", ALL)
        .AddAttachment((Attachment) { "directionalShadowmap", AttachmentFormat::DEPTH,         AttachmentPurpose::DEPTH,         AttachmentAccess::WRITE });
        .AddSubpass(new DirectionalShadowMapShader())

    Renderpass pointShadowMapPass = Renderpass()
        .AddAttachment(DEPTH_STENCIL, DEPTH_STENCIL, WRITE, "directionalShadowmap")
        .AddSubpass(new DirectionalShadowMapShader());
    */

    Renderpass geometryPass = Renderpass("geometry pass", OPAQUE)
        .AddAttachment((Attachment) { "g_depth",              AttachmentFormat::DEPTH_STENCIL, AttachmentPurpose::DEPTH_STENCIL, AttachmentAccess::WRITE,     })
        .AddAttachment((Attachment) { "g_position",           AttachmentFormat::FLOAT_3,       AttachmentPurpose::COLOR,         AttachmentAccess::READ_WRITE }) // change to depth only, we can calc pos from depth
        .AddAttachment((Attachment) { "g_normals",            AttachmentFormat::FLOAT_3,       AttachmentPurpose::COLOR,         AttachmentAccess::READ_WRITE })
        .AddAttachment((Attachment) { "g_albedo",             AttachmentFormat::FLOAT_4,       AttachmentPurpose::COLOR,         AttachmentAccess::READ_WRITE })
        .AddAttachment((Attachment) { "g_specular",           AttachmentFormat::FLOAT_1,       AttachmentPurpose::COLOR,         AttachmentAccess::READ_WRITE })
        .AddSubpass(new Shader("../src/shaders/g_buf.vert", "../src/shaders/g_buf.geom", "../src/shaders/g_buf.frag"));

    Renderpass compositionPass = Renderpass("composition pass", SCREEN_QUAD)
        .AddAttachment((Attachment) { "output",   AttachmentFormat::FLOAT_4,       AttachmentPurpose::COLOR,         AttachmentAccess::READ_WRITE })
        .RequestAttachmentFromPipeline({ "g_albedo",   AS_TEXTURE, "tex_diffuse"  })
        .RequestAttachmentFromPipeline({ "g_position", AS_TEXTURE, "tex_pos"      })
        .RequestAttachmentFromPipeline({ "g_normals",  AS_TEXTURE, "tex_normal"   })
        .RequestAttachmentFromPipeline({ "g_specular", AS_TEXTURE, "tex_specular" })
        .AddSubpass(new Shader("../src/shaders/deferred_lighting.vert", "../src/shaders/deferred_lighting.frag"));

    Renderpass postProFxPass = Renderpass("postProFx pass", SCREEN_QUAD)
        .AddAttachment((Attachment) { "postfx_output",               AttachmentFormat::FLOAT_4,       AttachmentPurpose::COLOR,         AttachmentAccess::READ_WRITE })
        .RequestAttachmentFromPipeline({ "output", AS_TEXTURE, "tex" })
        .AddSubpass(new Shader("../src/shaders/texture.vert", "../src/shaders/pixelation.frag"));

    RenderpassSettings forwardTransparencySettings = RenderpassSettings::Default();
    forwardTransparencySettings.enable = { GL_BLEND, GL_DEPTH_TEST };
    forwardTransparencySettings.depthMask = GL_FALSE; // Test against existing depth map, but ignore self-depth
    forwardTransparencySettings.clear = 0;
    forwardTransparencySettings.clearColor = glm::vec4(0.f, 1.f, 0.f, 1.f);
    Renderpass forwardTransparencyPass = Renderpass("forward transparency pass", TRANSPARENT, forwardTransparencySettings)
        .RequestAttachmentFromPipeline( { "output",  AS_ATTACHMENT } )
        .RequestAttachmentFromPipeline( { "g_depth", AS_ATTACHMENT } )
        .AddSubpass(new Shader("../src/shaders/forward_transparency.vert", "../src/shaders/forward_transparency.frag"));

    Renderpass debugGbufPass = Renderpass("debug geometry pass", ALL)
        .AddAttachment((Attachment) { "debug_output",               AttachmentFormat::FLOAT_4,       AttachmentPurpose::COLOR,         AttachmentAccess::READ_WRITE })
        .RequestAttachmentFromPipeline({ "g_position", AS_TEXTURE })
        .RequestAttachmentFromPipeline({ "g_normals",  AS_TEXTURE })
        .RequestAttachmentFromPipeline({ "g_albedo",   AS_TEXTURE })
        .RequestAttachmentFromPipeline({ "g_specular", AS_TEXTURE })
        .AddSubpass(new Shader("../src/shaders/g_buf_debug.vert", "../src/shaders/g_buf_debug.frag"));

    // NOTE: Huh, turns out running braindead "blit" shaders is quite expensive. Outputing directly from composition pass affords us like 2ms
    // at least that was on an intel igpu
    pipeline = (RenderPipeline)
    {
        {
            geometryPass, 
            compositionPass,
            forwardTransparencyPass,
            postProFxPass,
            //debugGbufPass,
            Renderpass::OutputPass("postfx_output"), // output for the whole deferred pipeline shebang
            //Renderpass::OutputPass("debug_output") // on top of it all render debug info

            // FSR would be amazing to try, oh wait lol that's DX11/12 and vulkan only, fuck me
        }
    };
    assert(pipeline.InitializePipeline());

    Model transparentModel = Model("../assets/suzanne.obj");
    Model model = Model("../assets/sponza/sponza.obj");
    Material* opaqueMat = new OpaqueMaterial();
    for (auto& mesh : model.meshes)
    {
        mesh.transform = Transform(glm::vec3(0.f, -10.f, 0.f), glm::quat(glm::vec3(0.f, 0.f, 0.f)), glm::vec3(5.f, 5.f, 5.f));
        scene.meshes[mesh.meshTag].push_back({ mesh, opaqueMat });
    }

    TransparentMaterial* transparentMat = new TransparentMaterial();
    transparentMat->resourceData.opacity = 0.1f;
    transparentMat->resourceData.r = 1.0f;
    transparentMat->resourceData.g = 0.0f;
    transparentMat->resourceData.b = 0.0f;
    for (auto& mesh : transparentModel.meshes)
    {
        mesh.transform = Transform(glm::vec3(0.f, 200.f, 0.f), glm::quat(glm::vec3(0.f, 0.f, 0.f)), glm::vec3(100.f, 100.f, 100.f));
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

    for (int i = 0; i < pipeline.passes.size(); i++)
    {
        Renderpass& renderpass = pipeline.passes[i];

        for (int j = 0; i > 0 && j < pipeline.passes[i-1].settings.enable.size(); j++)
            glDisable(pipeline.passes[i-1].settings.enable[j]);
        for (int j = 0; j < renderpass.settings.enable.size(); j++)
            glEnable(renderpass.settings.enable[j]);
        glCullFace(renderpass.settings.cullFace);
        glDepthMask(renderpass.settings.depthMask);
        glBlendFunc(renderpass.settings.srcBlendFactor, renderpass.settings.dstBlendFactor);

        glBindFramebuffer(GL_FRAMEBUFFER, renderpass.fbo);
        glClearColor(renderpass.settings.clearColor.x, renderpass.settings.clearColor.y,
            renderpass.settings.clearColor.z, renderpass.settings.clearColor.a);
        glClear(renderpass.settings.clear);

        for (RequestedAttachment& attachmentRequest : renderpass.requestedAttachments)
        {
            Attachment* attachment = pipeline.RetrieveAttachment(attachmentRequest.name);
            // AS_TEXTURE handled on a per-shader basis below
            if (!attachment || attachmentRequest.requestAs != AS_BLIT)
                continue;

            // TODO
            assert(false);
        }

        for (Shader* subpass : renderpass.subpasses)
        {
            int activatedTextureCount = 0;
            subpass->Use();

            for (RequestedAttachment& attachmentRequest : renderpass.requestedAttachments)
            {
                Attachment* attachment = pipeline.RetrieveAttachment(attachmentRequest.name);
                // AS_BLIT handled on a per-renderpass basis above
                if (attachment == nullptr || attachmentRequest.requestAs != AS_TEXTURE)
                    continue;

                // TODO: Absolutely idiotic this. Should have a MRU eviction policy cache for texture units
                // ... maybe MRU is not the best choice? No clue, needs testing
                glActiveTexture(GL_TEXTURE0 + activatedTextureCount);
                subpass->SetUniform(attachmentRequest.bindAs, activatedTextureCount++);
                glBindTexture(GL_TEXTURE_2D, attachment->id);
            }

            subpass->AddDummyForUnboundTextures(pipeline.dummyTextureUnit);
            for (MeshWithMaterial& meshWithMaterial : scene.meshes[renderpass.acceptedMeshTags])
            {
                // TODO: not necessary if already bound
                //       even if bound and changed can only partially update
                meshWithMaterial.material->Bind();
                meshWithMaterial.material->UpdateData();

                glm::mat4 model = meshWithMaterial.mesh.transform.Model();
                glBindBuffer(GL_UNIFORM_BUFFER, ubo);
                glBufferData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), &model, GL_STATIC_DRAW);
                glBindBuffer(GL_UNIFORM_BUFFER, 0);

                // TODO: Should be reworked - there should be some sort of communication between the shader and model here.
                // Shader dictates what textures it needs, the model provides them
                meshWithMaterial.mesh.Render(*subpass);
            }
        }
    }
}
