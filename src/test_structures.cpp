#include "test_structures.h"

#include "model.h"
#include "mesh.h"
#include "material.h"

Scene TestScene() 
{
    Scene scene; 

    // Lights
    {
        scene.directionalLight.color = glm::vec3(1.f, 1.f, 1.f);
        scene.directionalLight.transform = Transform(glm::vec3(0, 12000, 0), glm::quat(0.7, 0, 0, 0.7));
        scene.lighting.directionalBiasAndAngleBias = glm::vec4(0.0005, 0.0005, 0, 0);
        {
            glm::mat4 proj = glm::ortho(-3000.f, 3000.f, -1000.f, 8000.f, scene.camera.nearClippingPlane, scene.camera.farClippingPlane);
            glm::mat4 view = glm::lookAt(glm::vec3(5000.f, 10000.f, 1000.f), glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 1.f, 0.f));
            scene.directionalLight.viewProjection = proj * view;
            scene.lighting.directionalLightViewProjection = scene.directionalLight.viewProjection;

            scene.lighting.directionalLightDir = glm::normalize(glm::vec4(5000.f, 10000.f, 1000.f, 0.f) * -1.f);
        }
    }

    scene.sceneParams.gamma = 1.5f; // sRGB = 2.2
    scene.sceneParams.wireframe = 0;
    scene.sceneParams.specularPower = 32.f;
    scene.sceneParams.viewportWidth = 1920.f;
    scene.sceneParams.viewportHeight = 1080.f;

    Model transparentModel = Model("../assets/dragon.obj");
    Model sponza = Model("../assets/sponza/sponza.obj");
    Model backpack = Model("../assets/backpack/backpack.obj");
    Material* opaqueMat = new OpaqueMaterial();
    for (auto& mesh : sponza.meshes)
    {
        mesh.transform = Transform(glm::vec3(0.f, -10.f, 0.f), glm::quat(glm::vec3(0.f, 0.f, 0.f)), glm::vec3(5.f, 5.f, 5.f));
        scene.meshes[mesh.meshTag].push_back({ mesh, opaqueMat });
    }
    for (auto& mesh : backpack.meshes)
    {
        mesh.transform = Transform(glm::vec3(500.f, 200.f, -700.f), glm::quat(glm::vec3(0.f, 0.f, 0.f)), glm::vec3(75.f, 75.f, 75.f));
        scene.meshes[mesh.meshTag].push_back({ mesh, opaqueMat });
    }

    TransparentMaterial* blueTransparentMat = new TransparentMaterial(0.f, 0.f, 1.f, 0.2f);
    blueTransparentMat->Bind();
    blueTransparentMat->UpdateData();
    TransparentMaterial* greenTransparentMat = new TransparentMaterial(0.f, 1.f, 0.f, 0.4f);
    greenTransparentMat->Bind();
    greenTransparentMat->UpdateData();
    TransparentMaterial* redTransparentMat = new TransparentMaterial(1.f, 0.f, 0.f, 0.7f);
    redTransparentMat->Bind();
    redTransparentMat->UpdateData();
    for (auto& mesh : transparentModel.meshes)
    {
        mesh.transform = Transform(glm::vec3(0.f, 200.f, 0.f), glm::quat(glm::vec3(0.f, 0.f, 0.f)), glm::vec3(750.f, 750.f, 750.f));
        scene.meshes[TRANSPARENT].push_back({ mesh, blueTransparentMat });
        mesh.transform = Transform(glm::vec3(500.f, 200.f, 0.f), glm::quat(glm::vec3(0.f, 0.f, 0.f)), glm::vec3(750.f, 750.f, 750.f));
        scene.meshes[TRANSPARENT].push_back({ mesh, greenTransparentMat });
        mesh.transform = Transform(glm::vec3(-500.f, 200.f, 0.f), glm::quat(glm::vec3(0.f, 0.f, 0.f)), glm::vec3(750.f, 750.f, 750.f));
        scene.meshes[TRANSPARENT].push_back({ mesh, redTransparentMat });
    }

    scene.meshes[SCREEN_QUAD].push_back({ Mesh::ScreenQuadMesh(), new EmptyMaterial() });

    return scene;
}

RenderPipeline UnconfiguredDeferredPipeline(ShaderPool& shaders)
{
    RenderPipeline pipeline;

    Shader& directionalShadowmapGenShader = shaders.AddShader("directional shadowmap gen",
        ShaderDescriptor(
            {
                ShaderDescriptor::File(SHADER_PATH "directional_shadowmap_gen.vert", ShaderDescriptor::VERTEX_SHADER),
                ShaderDescriptor::File(SHADER_PATH "directional_shadowmap_gen.frag", ShaderDescriptor::FRAGMENT_SHADER)
            }));

    Renderpass& directionalShadowmapGenPass = pipeline.AddPass("Directional shadowmap gen pass");
    RenderpassAttachment& shadowmap = directionalShadowmapGenPass.AddAttachment(RenderpassAttachment("shadowmap", AttachmentFormat::DEPTH));
    directionalShadowmapGenPass.AddSubpass("Gen subpass", &directionalShadowmapGenShader, OPAQUE,
        {
            SubpassAttachment(&directionalShadowmapGenPass.AddAttachment(RenderpassAttachment("fuck you", AttachmentFormat::FLOAT_4)), SubpassAttachment::AS_COLOR),
            SubpassAttachment(&shadowmap, SubpassAttachment::AS_DEPTH)
        });

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
                ShaderDescriptor::File(SHADER_PATH "fallthrough.vert", ShaderDescriptor::VERTEX_SHADER),
                ShaderDescriptor::File(SHADER_PATH "deferred_lighting.frag", ShaderDescriptor::FRAGMENT_SHADER)
            }));

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
            SubpassAttachment(&shadowmap,        SubpassAttachment::AS_TEXTURE, "shadow_map"),
        });

    return pipeline;
}

RenderPipeline NoTransparencyPipeline(ShaderPool& shaders)
{
    RenderPipeline pipeline = UnconfiguredDeferredPipeline(shaders);
    pipeline.AddOutputPass(shaders);

    assert(pipeline.ConfigureAttachments());

    return pipeline;
}

RenderPipeline UnsortedForwardTransparencyPipeline(ShaderPool& shaders)
{
    Shader& forwardTransparencyShader = shaders.AddShader("forward transparency",
        ShaderDescriptor(
            {
                ShaderDescriptor::File(SHADER_PATH "default.vert", ShaderDescriptor::VERTEX_SHADER),
                ShaderDescriptor::File(SHADER_PATH "forward_transparency.frag", ShaderDescriptor::FRAGMENT_SHADER)
            }));
    RenderPipeline pipeline = UnconfiguredDeferredPipeline(shaders);

    pipeline.AddOutputPass(shaders);

    PassSettings settings = PassSettings::DefaultOutputRenderpassSettings();
    settings.ignoreApplication = false;
    settings.ignoreClear = true;
    settings.enable = { GL_BLEND, };
    settings.srcBlendFactor = GL_SRC_ALPHA;
    settings.dstBlendFactor = GL_ONE_MINUS_SRC_ALPHA;
    Renderpass& forwardTransparencyRenderpass = pipeline.AddPass("Forward transparency pass", settings);
    forwardTransparencyRenderpass.fbo = 0;

    Subpass& subpass = forwardTransparencyRenderpass.AddSubpass("Forward transparency subass", &forwardTransparencyShader, TRANSPARENT, 
            {}, settings);

    assert(pipeline.ConfigureAttachments());
    return pipeline;
}

RenderPipeline DepthPeelingPipeline(ShaderPool& shaders)
{
    Shader& depthPeelingShader = shaders.AddShader("depth peeling", 
        ShaderDescriptor(
            {
                ShaderDescriptor::File(SHADER_PATH "default.vert", ShaderDescriptor::VERTEX_SHADER),
                ShaderDescriptor::File(SHADER_PATH "depth_peeling.frag", ShaderDescriptor::FRAGMENT_SHADER)
            }));
    
    RenderPipeline pipeline = UnconfiguredDeferredPipeline(shaders);

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

    pipeline.AddOutputPass(shaders);

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

        Subpass& subpass = outputPass.AddSubpass(subpassName, &shaders.GetShader(SCREEN_QUAD_TEXTURE_SHADER), SCREEN_QUAD,
                {
                // Default framebuffer already has a color attachment, no need to add another one
                SubpassAttachment(&depthPeelingPass.GetAttachment(layer), SubpassAttachment::AS_TEXTURE, "tex")
                }, settings);
    }

    assert(pipeline.ConfigureAttachments());
    return pipeline;
}

RenderPipeline DualDepthPeelingPipeline(ShaderPool& shaders)
{
    Shader& dualDepthPeelingInitShader = shaders.AddShader("dual depth peeling init",
        ShaderDescriptor(
            {
                ShaderDescriptor::File(SHADER_PATH "default.vert", ShaderDescriptor::VERTEX_SHADER),
                ShaderDescriptor::File(SHADER_PATH "dual_depth_peeling_init.frag", ShaderDescriptor::FRAGMENT_SHADER),
            }));
    Shader& dualDepthPeelingShader = shaders.AddShader("dual depth peeling",
        ShaderDescriptor(
            {
                ShaderDescriptor::File(SHADER_PATH "default.vert", ShaderDescriptor::VERTEX_SHADER),
                ShaderDescriptor::File(SHADER_PATH "dual_depth_peeling.frag", ShaderDescriptor::FRAGMENT_SHADER),
            }));
    Shader& dualDepthPeelingBackBlendingShader = shaders.AddShader("dual depth peeling back blending",
        ShaderDescriptor(
            {
                ShaderDescriptor::File(SHADER_PATH "fallthrough.vert", ShaderDescriptor::VERTEX_SHADER),
                ShaderDescriptor::File(SHADER_PATH "dual_depth_peeling_back_blending.frag", ShaderDescriptor::FRAGMENT_SHADER),
            }));
    Shader& dualDepthPeelingCompositionShader = shaders.AddShader("dual depth peeling composition",
        ShaderDescriptor(
            {
                ShaderDescriptor::File(SHADER_PATH "fallthrough.vert", ShaderDescriptor::VERTEX_SHADER),
                ShaderDescriptor::File(SHADER_PATH "dual_depth_peeling_composition.frag", ShaderDescriptor::FRAGMENT_SHADER),
            }));

    RenderPipeline pipeline = UnconfiguredDeferredPipeline(shaders);

#define DUAL_DEPTH_PASS_COUNT 3
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
    for (int i = 1; i <= DUAL_DEPTH_PASS_COUNT; i++)
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

    pipeline.AddOutputPass(shaders);

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
                SubpassAttachment(DUAL_DEPTH_PASS_COUNT % 2 == 0 ? &frontBlenderA : &frontBlenderB, SubpassAttachment::AS_TEXTURE, "frontBlender"),
                SubpassAttachment(&finalBackBlender, SubpassAttachment::AS_TEXTURE, "backBlender"),
            }, settings);

    assert(pipeline.ConfigureAttachments());
    return pipeline;
}

std::vector<NamedPipeline> TestPipelines(ShaderPool& shaders)
{
    return 
        {
            { "No transparency", NoTransparencyPipeline(shaders) },
            { "Unsorted forward transparency", UnsortedForwardTransparencyPipeline(shaders) },
            { "Depth peeling", DepthPeelingPipeline(shaders) },
            { "Dual depth peeling", DualDepthPeelingPipeline(shaders) },
        };
}
