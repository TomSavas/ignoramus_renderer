#include "test_structures.h"

#include "model.h"
#include "mesh.h"
#include "material.h"
#include "log.h"
#include "particle_sys.h"
#include "random.h"

#define STRINGIFY(x) #x
#define STRINGIFY_VALUE(x) STRINGIFY(x) 

void AddModel(Model& model, Transform& transform, MeshTag meshTag, Material* material, Scene& scene)
{
    for (auto& mesh : model.meshes)
    {
        mesh.transform = transform;
        scene.meshes[meshTag].push_back({ mesh, material });
    }
}

void AddProxyAABBModel(Model& proxyModel, Transform& proxiedTransform, AABB& proxiedAABB, Material* material, Scene& scene)
{
    Transform transform = proxiedTransform;
    transform.scale *= proxiedAABB.Extents() * 2.f;
    AddModel(proxyModel, transform, PROXY, material, scene);
}

Scene TestScene() 
{
    Scene scene; 

    // Fictitious pipeline that just helps set up the global attachments
    RenderPipeline auxiliaryPipeline;
    auxiliaryPipeline.passes.push_back(&scene.globalAttachments);
#define POINT_LIGHTS "PointLights"
    scene.globalAttachments.AddAttachment(RenderpassAttachment::SSBO(POINT_LIGHTS, sizeof(Scene::Lights)));
    scene.globalAttachments.AddDefine(STRINGIFY(MAX_POINT_LIGHTS), STRINGIFY_VALUE(MAX_POINT_LIGHTS));
#define LIGHT_TILE_CULLING_SUBPASS "Light tile culling subpass"
#define LIGHT_TILE_CULLING_GROUP_SIZE 16
    scene.globalAttachments.AddDefine("LIGHT_TILE_CULLING_WORK_GROUP_SIZE_X", STRINGIFY_VALUE(LIGHT_TILE_CULLING_GROUP_SIZE));
    scene.globalAttachments.AddDefine("LIGHT_TILE_CULLING_WORK_GROUP_SIZE_Y", STRINGIFY_VALUE(LIGHT_TILE_CULLING_GROUP_SIZE));
    PassSettings lightTileCullingSettings = PassSettings::DefaultSubpassSettings();
#define LIGHT_TILE_COUNT_X 48
#define LIGHT_TILE_COUNT_Y 32
#define LIGHT_TILE_COUNT (LIGHT_TILE_COUNT_X * LIGHT_TILE_COUNT_Y)
    scene.globalAttachments.AddDefine(STRINGIFY(LIGHT_TILE_COUNT_X), STRINGIFY_VALUE(LIGHT_TILE_COUNT_X));
    scene.globalAttachments.AddDefine(STRINGIFY(LIGHT_TILE_COUNT_Y), STRINGIFY_VALUE(LIGHT_TILE_COUNT_Y));
    scene.globalAttachments.AddDefine(STRINGIFY(LIGHT_TILE_COUNT), LIGHT_TILE_COUNT);
#define LIGHT_TILE_DATA "LightTileData"
    scene.globalAttachments.AddAttachment(RenderpassAttachment::SSBO("LightTileData", LIGHT_TILE_COUNT * sizeof(unsigned int) * 2));
#define LIGHT_IDS "LightIds"
    scene.globalAttachments.AddAttachment(RenderpassAttachment::SSBO("LightIds", LIGHT_TILE_COUNT * sizeof(unsigned int) * MAX_POINT_LIGHTS));
#define LIGHT_ID_COUNT "lightIdCount"
    scene.globalAttachments.AddAttachment(RenderpassAttachment::AtomicCounter("lightIdCount"));
    auxiliaryPipeline.ConfigureAttachments(false);

    // Point lights
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

        for (int i = 0; i < MAX_POINT_LIGHTS; i++)
        {
            glm::vec3 color = glm::vec3(RandomFloat(), RandomFloat(), RandomFloat());
            glm::vec3 pos ((RandomFloat() - 0.5) * 10000.f, 50.f + RandomFloat() * 4000.f, (RandomFloat() - 0.5f) * 10000.f);
            float radius = 100.f + RandomFloat() * 600;
            scene.lights.pointLights[scene.lights.pointLightCount++] = Scene::PointLight(color, pos, radius);
        }
        
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, scene.globalAttachments.GetAttachment(POINT_LIGHTS).id);
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(Scene::Lights), &scene.lights, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

    scene.sceneParams.gamma = 1.5f; // sRGB = 2.2
    scene.sceneParams.wireframe = 0;
    scene.sceneParams.specularPower = 32.f;
    scene.sceneParams.viewportWidth = 1920.f;
    scene.sceneParams.viewportHeight = 1080.f;

    Material* proxyMat = new EmptyMaterial();
    Material* opaqueMat = new OpaqueMaterial();
    TransparentMaterial* blueTransparentMat = new TransparentMaterial(0.f, 0.f, 1.f, 0.1f, 256.f, 10.f);
    blueTransparentMat->Bind();
    blueTransparentMat->UpdateData();
    TransparentMaterial* greenTransparentMat = new TransparentMaterial(0.f, 1.f, 0.f, 0.2f, 256.f, 10.f);
    greenTransparentMat->Bind();
    greenTransparentMat->UpdateData();
    TransparentMaterial* redTransparentMat = new TransparentMaterial(1.f, 0.f, 0.f, 0.4f, 256.f, 10.f);
    redTransparentMat->Bind();
    redTransparentMat->UpdateData();

    Model dragon = Model("../assets/dragon.obj");
    Model sponza = Model("../assets/sponza/sponza.obj");
    Model sphere = Model("../assets/sphere/sphere.obj");
    Model cube = Model("../assets/cube/cube.obj");

    Transform sponzaTransform(glm::vec3(0.f, -10.f, 0.f), glm::quat(glm::vec3(0.f, 0.f, 0.f)), glm::vec3(5.f, 5.f, 5.f));
    AddModel(sponza, sponzaTransform, OPAQUE, opaqueMat, scene);

    Transform opaqueDragonTransform(glm::vec3(0.f, 200.f, -700.f), glm::quat(glm::vec3(0.f, -glm::pi<float>(), 0.f)), glm::vec3(500.f));
    AddModel(dragon, opaqueDragonTransform, OPAQUE, opaqueMat, scene);
    AddProxyAABBModel(cube, opaqueDragonTransform, dragon.aabbModelSpace, proxyMat, scene);

    Transform transparentBlueDragonTransform(glm::vec3(0.f, 200.f, 100.f), glm::quat(glm::vec3(0.f, 0.f, 0.f)), glm::vec3(500.f));
    AddModel(dragon, transparentBlueDragonTransform, TRANSPARENT, blueTransparentMat, scene);
    AddProxyAABBModel(cube, transparentBlueDragonTransform, dragon.aabbModelSpace, proxyMat, scene);

    Transform transparentGreenDragonTransform(glm::vec3(500.f, 200.f, 100.f), glm::quat(glm::vec3(0.f, 0.f, 0.f)), glm::vec3(500.f));
    AddModel(dragon, transparentGreenDragonTransform, TRANSPARENT, greenTransparentMat, scene);
    AddProxyAABBModel(cube, transparentGreenDragonTransform, dragon.aabbModelSpace, proxyMat, scene);

    Transform transparentRedDragonTransform(glm::vec3(-500.f, 200.f, 100.f), glm::quat(glm::vec3(0.f, 0.f, 0.f)), glm::vec3(500.f));
    AddModel(dragon, transparentRedDragonTransform, TRANSPARENT, redTransparentMat, scene);
    AddProxyAABBModel(cube, transparentRedDragonTransform, dragon.aabbModelSpace, proxyMat, scene);

    Transform greenTransforms[] =
    {
        Transform(glm::vec3(1500.f + 500.f + 400.f, 200.f, 100.f), glm::quat(glm::vec3(0.f, 0.f, 0.f)), glm::vec3(1.f)),
        Transform(glm::vec3(-1500.f + 500.f, 200.f, -600.f), glm::quat(glm::vec3(0.f, 0.f, 0.f)), glm::vec3(1.f)),
        Transform(glm::vec3(-1500.f + 500.f, 200.f, -600.f), glm::quat(glm::vec3(0.f, 0.f, 0.f)), glm::vec3(0.95f))
    };
    Transform blueTransforms[] =
    {
        Transform(glm::vec3(1500.f + 0.f + 200.f, 200.f, 100.f), glm::quat(glm::vec3(0.f, 0.f, 0.f)), glm::vec3(1.f)),
        Transform(glm::vec3(-1500.f + 0.f - 200.f, 200.f, -600.f), glm::quat(glm::vec3(0.f, 0.f, 0.f)), glm::vec3(1.f)),
        Transform(glm::vec3(-1500.f + 0.f - 200.f, 200.f, -600.f), glm::quat(glm::vec3(0.f, 0.f, 0.f)), glm::vec3(0.95f))
    };
    Transform redTransforms[] =
    {
        Transform(glm::vec3(1500.f + -500.f, 200.f, 100.f), glm::quat(glm::vec3(0.f, 0.f, 0.f)), glm::vec3(1.f)),
        Transform(glm::vec3(-1500.f + -500.f - 400.f, 200.f, -600.f), glm::quat(glm::vec3(0.f, 0.f, 0.f)), glm::vec3(1.f)),
        Transform(glm::vec3(-1500.f + -500.f - 400.f, 200.f, -600.f), glm::quat(glm::vec3(0.f, 0.f, 0.f)), glm::vec3(0.95f))
    };

    for (int i = 0; i < 3; i++)
    {
        AddModel(sphere, greenTransforms[i], TRANSPARENT, greenTransparentMat, scene);
        AddProxyAABBModel(cube, greenTransforms[i], sphere.aabbModelSpace, proxyMat, scene);

        AddModel(sphere, blueTransforms[i], TRANSPARENT, blueTransparentMat, scene);
        AddProxyAABBModel(cube, blueTransforms[i], sphere.aabbModelSpace, proxyMat, scene);

        AddModel(sphere, redTransforms[i], TRANSPARENT, redTransparentMat, scene);
        AddProxyAABBModel(cube, redTransforms[i], sphere.aabbModelSpace, proxyMat, scene);
    }

    scene.meshes[SCREEN_QUAD].push_back({ Mesh::ScreenQuadMesh(), new EmptyMaterial() });

    // Idiotic. Need this fast though, so will work for now
    char* smokeTextures[10];
    for (int i = 0; i < 10; i++)
    {
        smokeTextures[i] = (char*) malloc(sizeof(char) * 32);
        sprintf(smokeTextures[i], "../assets/textures/smoke_%02d.png", i % 10 + 1);
    }

#define PARTICLE_COUNT 300
    std::vector<MeshWithMaterial*> particles;
    for (int i = 0; i < PARTICLE_COUNT; i++)
    {
        Mesh mesh = Mesh::ScreenQuadMesh();
        mesh.meshTag = PARTICLE;
        mesh.aabbModelSpace = AABB(glm::vec3(-1.f, -1.f, 0.f), glm::vec3(1.f, 1.f, 0.f));
        mesh.transform.scale = glm::vec3(300.f);
        mesh.textures["particle_tex"] = smokeTextures[i % 10];
        // Only shade particles in one cluster, leave the other unshaded
        scene.meshes[PARTICLE].push_back({ mesh, new TransparentMaterial(.1f, .1f, .1f, .75f, 6.f, 6.f, i < PARTICLE_COUNT/2, true) });
    }


    for (int i = 0; i < PARTICLE_COUNT/2; i++)
    {
        particles.push_back(&scene.meshes[PARTICLE][scene.meshes[PARTICLE].size() - PARTICLE_COUNT + i]);
    }
    glm::vec4 color(0.75f, 0.75f, 0.75f, 1.f);
    AABB spawnZone(glm::vec3(3500.f, 0.f, -500.f), glm::vec3(4100.f, 0.f, 100.f));
    scene.particleSystems.push_back(ParticleSys(particles, spawnZone, color, PARTICLE_COUNT/2, 2.5f, 1000.f));
    AABB particleAABB = AABB(spawnZone.min + glm::vec3(0.f, 700.f, 0.f), spawnZone.max + glm::vec3(0.f, 700.f, 0.f));
    Transform particleSystemCenter = Transform(particleAABB.min + particleAABB.Extents());
    AddProxyAABBModel(cube, particleSystemCenter, particleAABB, proxyMat, scene);
    particleSystemCenter.scale = glm::vec3(1400.f);
    AddModel(cube, particleSystemCenter, PROXY, opaqueMat, scene);
    particles.clear();

    for (int i = 0; i < PARTICLE_COUNT/2; i++)
    {
        particles.push_back(&scene.meshes[PARTICLE][scene.meshes[PARTICLE].size() - PARTICLE_COUNT/2 + i]);
    }
    spawnZone = AABB(glm::vec3(-4600.f, 0.f, -500.f), glm::vec3(-4200.f, 1.f, 100.f));
    scene.particleSystems.push_back(ParticleSys(particles, spawnZone, color, PARTICLE_COUNT/2, 2.5f, 1000.f));
    particleAABB = AABB(spawnZone.min + glm::vec3(0.f, 700.f, 0.f), spawnZone.max + glm::vec3(0.f, 700.f, 0.f));
    particleSystemCenter = Transform(particleAABB.min + particleAABB.Extents());
    AddProxyAABBModel(cube, particleSystemCenter, particleAABB, proxyMat, scene);
    particleSystemCenter.scale = glm::vec3(1500.f);
    AddModel(cube, particleSystemCenter, PROXY, opaqueMat, scene);

    return scene;
}

struct PipelineWithShadowmap
{
    RenderPipeline pipeline;
    RenderpassAttachment* shadowmap;
};

PipelineWithShadowmap UnconfiguredDeferredPipeline(Renderpass& globalAttachments, ShaderPool& shaders)
{
    RenderPipeline pipeline;

    Renderpass& directionalShadowmapGenPass = pipeline.AddPass("Directional shadowmap gen pass");
    RenderpassAttachment& shadowmap = directionalShadowmapGenPass.AddAttachment(RenderpassAttachment("shadowmap", AttachmentFormat::DEPTH));
    Shader& directionalShadowmapGenShader = shaders.GetShader(ShaderDescriptor(
        {
            ShaderDescriptor::File(SHADER_PATH "directional_shadowmap_gen.vert", ShaderDescriptor::VERTEX_SHADER),
            ShaderDescriptor::File(SHADER_PATH "empty.frag", ShaderDescriptor::FRAGMENT_SHADER)
        }, globalAttachments.DefineValues()));
    directionalShadowmapGenPass.AddSubpass("Gen subpass", &directionalShadowmapGenShader, OPAQUE,
        {
            // TODO: remove this color attachment
            SubpassAttachment(&directionalShadowmapGenPass.AddAttachment(RenderpassAttachment("unnecessary color", AttachmentFormat::FLOAT_4)), SubpassAttachment::AS_COLOR),
            SubpassAttachment(&shadowmap, SubpassAttachment::AS_DEPTH)
        });

    // Proxy geometry
    Shader& noShadingShader = shaders.GetShader(
        ShaderDescriptor(
            {
                ShaderDescriptor::File(SHADER_PATH "default.vert", ShaderDescriptor::VERTEX_SHADER),
                ShaderDescriptor::File(SHADER_PATH "empty.frag", ShaderDescriptor::FRAGMENT_SHADER)
        }, globalAttachments.DefineValues()));

    Renderpass& proxyDepthRenderpass = pipeline.AddPass("Proxy geometry min depth pass");
    RenderpassAttachment& proxyMinDepth = proxyDepthRenderpass.AddAttachment(RenderpassAttachment("proxy_min_depth", AttachmentFormat::DEPTH));
    Subpass& subpass = proxyDepthRenderpass.AddSubpass("Forward transparency subpass", &noShadingShader, PROXY, 
        {
            SubpassAttachment(&proxyDepthRenderpass.AddAttachment(RenderpassAttachment("unnecessary color", AttachmentFormat::FLOAT_4)), SubpassAttachment::AS_COLOR),
            SubpassAttachment(&proxyMinDepth, SubpassAttachment::AS_DEPTH)
        });


#define DEFERRED_PASS "Deferred pass"
    Renderpass& deferredLightingPass = pipeline.AddPass(DEFERRED_PASS);
    RenderpassAttachment& deferredDepth    = deferredLightingPass.AddAttachment(RenderpassAttachment("g_depth",    AttachmentFormat::DEPTH));
    RenderpassAttachment& deferredPosition = deferredLightingPass.AddAttachment(RenderpassAttachment("g_position", AttachmentFormat::FLOAT_3));
    RenderpassAttachment& deferredNormal   = deferredLightingPass.AddAttachment(RenderpassAttachment("g_normal",   AttachmentFormat::FLOAT_3));
    RenderpassAttachment& deferredAlbedo   = deferredLightingPass.AddAttachment(RenderpassAttachment("g_albedo",   AttachmentFormat::FLOAT_3));
    RenderpassAttachment& deferredSpecular = deferredLightingPass.AddAttachment(RenderpassAttachment("g_specular", AttachmentFormat::FLOAT_3));

    Shader& geometryShader = shaders.GetShader(ShaderDescriptor(
        {
            ShaderDescriptor::File(SHADER_PATH "geometry_buffer.geom", ShaderDescriptor::GEOMETRY_SHADER),
            ShaderDescriptor::File(SHADER_PATH "geometry_buffer.vert", ShaderDescriptor::VERTEX_SHADER),
            ShaderDescriptor::File(SHADER_PATH "geometry_buffer.frag", ShaderDescriptor::FRAGMENT_SHADER)
        }, globalAttachments.DefineValues()));
#define GEOMETRY_SUBPASS "Geometry subpass"
    deferredLightingPass.AddSubpass(GEOMETRY_SUBPASS, &geometryShader, OPAQUE,
        {
            SubpassAttachment(&deferredDepth,    SubpassAttachment::AS_DEPTH),
            SubpassAttachment(&deferredPosition, SubpassAttachment::AS_COLOR),
            SubpassAttachment(&deferredNormal,   SubpassAttachment::AS_COLOR),
            SubpassAttachment(&deferredAlbedo,   SubpassAttachment::AS_COLOR),
            SubpassAttachment(&deferredSpecular, SubpassAttachment::AS_COLOR),
        });

    PassSettings lightTileCullingSettings = PassSettings::DefaultSubpassSettings();
    lightTileCullingSettings.computeWorkGroups = glm::ivec3(LIGHT_TILE_COUNT_X, LIGHT_TILE_COUNT_Y, 1);
    Shader& lightTileCullingShader = shaders.GetShader(ShaderDescriptor(
        {
            ShaderDescriptor::File(SHADER_PATH "light_tile_culling.comp", ShaderDescriptor::COMPUTE_SHADER),
        }, globalAttachments.DefineValues(deferredLightingPass.DefineValues())));
    deferredLightingPass.AddSubpass(LIGHT_TILE_CULLING_SUBPASS, &lightTileCullingShader, COMPUTE,
        {
            SubpassAttachment(&deferredDepth,    SubpassAttachment::AS_TEXTURE, "tex_depth"),
            SubpassAttachment(&proxyMinDepth,    SubpassAttachment::AS_TEXTURE, "tex_proxy_depth"),
            SubpassAttachment(&globalAttachments.GetAttachment(POINT_LIGHTS), SubpassAttachment::AS_SSBO, POINT_LIGHTS),
            SubpassAttachment(&globalAttachments.GetAttachment(LIGHT_TILE_DATA), SubpassAttachment::AS_SSBO, LIGHT_TILE_DATA),
            SubpassAttachment(&globalAttachments.GetAttachment(LIGHT_IDS), SubpassAttachment::AS_SSBO, LIGHT_IDS),
            SubpassAttachment(&globalAttachments.GetAttachment(LIGHT_ID_COUNT), SubpassAttachment::AS_ATOMIC_COUNTER, LIGHT_ID_COUNT)
        }, lightTileCullingSettings);
#define COMPOSITION_LIGHTING_SUBPASS "Composition-lighting subpass"
    Shader& deferredLightingShader = shaders.GetShader(ShaderDescriptor(
        {
            ShaderDescriptor::File(SHADER_PATH "fallthrough.vert", ShaderDescriptor::VERTEX_SHADER),
            ShaderDescriptor::File(FRAG_COMMON_SHADER, ShaderDescriptor::FRAGMENT_SHADER),
            ShaderDescriptor::File(LIGHTING_COMMON_SHADER, ShaderDescriptor::FRAGMENT_SHADER),
            ShaderDescriptor::File(SHADER_PATH "deferred_lighting.frag", ShaderDescriptor::FRAGMENT_SHADER)
        }, globalAttachments.DefineValues(deferredLightingPass.DefineValues())));
    
    PassSettings settings = PassSettings::DefaultSubpassSettings();
    settings.ignoreApplication = false;
    settings.depthMask = GL_FALSE;
    deferredLightingPass.AddSubpass(COMPOSITION_LIGHTING_SUBPASS, &deferredLightingShader, SCREEN_QUAD,
        {
            // TODO: allow picking all existing renderpass' (specific subpass') attachments and re-binding them as textures with the same names?
            SubpassAttachment(&deferredLightingPass.AddOutputAttachment(), SubpassAttachment::AS_COLOR),
            SubpassAttachment(&deferredPosition, SubpassAttachment::AS_TEXTURE, "tex_pos"),
            SubpassAttachment(&deferredNormal,   SubpassAttachment::AS_TEXTURE, "tex_normal"),
            SubpassAttachment(&deferredAlbedo,   SubpassAttachment::AS_TEXTURE, "tex_diffuse"),
            SubpassAttachment(&deferredSpecular, SubpassAttachment::AS_TEXTURE, "tex_specular"),
            SubpassAttachment(&shadowmap,        SubpassAttachment::AS_TEXTURE, "shadow_map"),
            SubpassAttachment(&globalAttachments.GetAttachment(POINT_LIGHTS), SubpassAttachment::AS_SSBO, POINT_LIGHTS),
            SubpassAttachment(&globalAttachments.GetAttachment(LIGHT_TILE_DATA), SubpassAttachment::AS_SSBO, LIGHT_TILE_DATA),
            SubpassAttachment(&globalAttachments.GetAttachment(LIGHT_IDS), SubpassAttachment::AS_SSBO, LIGHT_IDS),
            SubpassAttachment(&globalAttachments.GetAttachment(LIGHT_ID_COUNT), SubpassAttachment::AS_ATOMIC_COUNTER, LIGHT_ID_COUNT)
        }, settings);

    return { pipeline, &shadowmap };
}

RenderPipeline NoTransparencyPipeline(Renderpass& globalAttachments, ShaderPool& shaders)
{
    RenderPipeline pipeline = UnconfiguredDeferredPipeline(globalAttachments, shaders).pipeline;
    pipeline.AddOutputPass(shaders);

    assert(pipeline.ConfigureAttachments());

    return pipeline;
}

RenderPipeline UnsortedForwardTransparencyPipeline(Renderpass& globalAttachments, ShaderPool& shaders)
{
    Shader& forwardTransparencyShader = shaders.GetShader(
        ShaderDescriptor(
            {
                ShaderDescriptor::File(SHADER_PATH "default.vert", ShaderDescriptor::VERTEX_SHADER),
                ShaderDescriptor::File(FRAG_COMMON_SHADER, ShaderDescriptor::FRAGMENT_SHADER),
                ShaderDescriptor::File(LIGHTING_COMMON_SHADER, ShaderDescriptor::FRAGMENT_SHADER),
                ShaderDescriptor::File(SHADER_PATH "forward_transparency.frag", ShaderDescriptor::FRAGMENT_SHADER)
        }, globalAttachments.DefineValues()));
    PipelineWithShadowmap pipelineWithShadowmap = UnconfiguredDeferredPipeline(globalAttachments, shaders);

    Renderpass* deferredPass;
    for (int i = 0; i < pipelineWithShadowmap.pipeline.passes.size() && deferredPass == nullptr; i++)
    {
        Renderpass* pass = pipelineWithShadowmap.pipeline.passes[i];
        if (pass == nullptr || strcmp(pass->name, DEFERRED_PASS) == 0)
        {
            deferredPass = pass;
            break;
        }
    }

    PassSettings settings = PassSettings::DefaultOutputRenderpassSettings();
    settings.ignoreClear = true;
    settings.enable = { GL_BLEND, GL_DEPTH_TEST };
    settings.depthMask = GL_FALSE;
    settings.srcBlendFactor = GL_SRC_ALPHA;
    settings.dstBlendFactor = GL_ONE_MINUS_SRC_ALPHA;
    Subpass& subpass = deferredPass->AddSubpass("Forward transparency subpass", &forwardTransparencyShader, (MeshTag)(TRANSPARENT | PARTICLE), 
            {
                SubpassAttachment(deferredPass->outputAttachment, SubpassAttachment::AS_COLOR),
                SubpassAttachment(&deferredPass->GetAttachment("g_depth"), SubpassAttachment::AS_DEPTH),
                SubpassAttachment(pipelineWithShadowmap.shadowmap, SubpassAttachment::AS_TEXTURE, "shadow_map"),
                SubpassAttachment(&globalAttachments.GetAttachment(POINT_LIGHTS), SubpassAttachment::AS_SSBO, POINT_LIGHTS),
                SubpassAttachment(&globalAttachments.GetAttachment(LIGHT_TILE_DATA), SubpassAttachment::AS_SSBO, LIGHT_TILE_DATA),
                SubpassAttachment(&globalAttachments.GetAttachment(LIGHT_IDS), SubpassAttachment::AS_SSBO, LIGHT_IDS),
                SubpassAttachment(&globalAttachments.GetAttachment(LIGHT_ID_COUNT), SubpassAttachment::AS_ATOMIC_COUNTER, LIGHT_ID_COUNT)
            }, settings);

    pipelineWithShadowmap.pipeline.AddOutputPass(shaders);

    assert(pipelineWithShadowmap.pipeline.ConfigureAttachments());
    return pipelineWithShadowmap.pipeline;
}

RenderPipeline WeightedBlendedTransparencyPipeline(PipelineWithShadowmap pipelineWithShadowmap, Renderpass& globalAttachments, ShaderPool& shaders, const char* weightedTransparencyShaderPath,
        MeshTag meshTag = (MeshTag)(TRANSPARENT | PARTICLE), bool configure = true)
{
    Shader& weightedTransparencyShader = shaders.GetShader(
        ShaderDescriptor(
            {
                ShaderDescriptor::File(SHADER_PATH "default.vert", ShaderDescriptor::VERTEX_SHADER),
                ShaderDescriptor::File(FRAG_COMMON_SHADER, ShaderDescriptor::FRAGMENT_SHADER),
                ShaderDescriptor::File(LIGHTING_COMMON_SHADER, ShaderDescriptor::FRAGMENT_SHADER),
                ShaderDescriptor::File(weightedTransparencyShaderPath, ShaderDescriptor::FRAGMENT_SHADER)
        }, globalAttachments.DefineValues()));
    Shader& weightedTransparencyBlendShader = shaders.GetShader(
        ShaderDescriptor(
            {
                ShaderDescriptor::File(SHADER_PATH "fallthrough.vert", ShaderDescriptor::VERTEX_SHADER),
                ShaderDescriptor::File(FRAG_COMMON_SHADER, ShaderDescriptor::FRAGMENT_SHADER),
                ShaderDescriptor::File(LIGHTING_COMMON_SHADER, ShaderDescriptor::FRAGMENT_SHADER),
                ShaderDescriptor::File(SHADER_PATH "weighted_transparency_blend.frag", ShaderDescriptor::FRAGMENT_SHADER)
        }, globalAttachments.DefineValues()));

    if (configure)
    {
        pipelineWithShadowmap.pipeline.AddOutputPass(shaders);
    }

    Renderpass* deferredPass;
    for (int i = 0; i < pipelineWithShadowmap.pipeline.passes.size() && deferredPass == nullptr; i++)
    {
        Renderpass* pass = pipelineWithShadowmap.pipeline.passes[i];
        if (pass == nullptr || strcmp(pass->name, DEFERRED_PASS) == 0)
        {
            deferredPass = pass;
            break;
        }
    }

    PassSettings settings = PassSettings::DefaultSubpassSettings();
    settings.ignoreApplication = false;
    settings.ignoreClear = true;
    settings.enable = { GL_BLEND, GL_DEPTH_TEST };
    settings.depthMask = GL_FALSE;
    settings.blendFactors.push_back({GL_ONE, GL_ONE});
    settings.blendFactors.push_back({GL_ZERO, GL_ONE_MINUS_SRC_COLOR});
    settings.blendEquation = GL_FUNC_ADD;

    Renderpass& weightedBlendedPass = pipelineWithShadowmap.pipeline.AddPass("Weighted blended transparency pass", settings);
    RenderpassAttachment& accumulator = weightedBlendedPass.AddAttachment(RenderpassAttachment("accumulator", AttachmentFormat::FLOAT_4, AttachmentClearOpts(glm::vec4(0.f))));
    RenderpassAttachment& revealage = weightedBlendedPass.AddAttachment(RenderpassAttachment("revealage", AttachmentFormat::FLOAT_1, AttachmentClearOpts(glm::vec4(1.f))));

    weightedBlendedPass.AddSubpass("Transparency subpass", &weightedTransparencyShader, meshTag, 
            {
                SubpassAttachment(&accumulator, SubpassAttachment::AS_COLOR),
                SubpassAttachment(&revealage, SubpassAttachment::AS_COLOR),
                SubpassAttachment(&deferredPass->GetAttachment("g_depth"), SubpassAttachment::AS_DEPTH),

                SubpassAttachment(pipelineWithShadowmap.shadowmap, SubpassAttachment::AS_TEXTURE, "shadow_map"),
                SubpassAttachment(&globalAttachments.GetAttachment(POINT_LIGHTS), SubpassAttachment::AS_SSBO, POINT_LIGHTS),
                SubpassAttachment(&globalAttachments.GetAttachment(LIGHT_TILE_DATA), SubpassAttachment::AS_SSBO, LIGHT_TILE_DATA),
                SubpassAttachment(&globalAttachments.GetAttachment(LIGHT_IDS), SubpassAttachment::AS_SSBO, LIGHT_IDS),
                SubpassAttachment(&globalAttachments.GetAttachment(LIGHT_ID_COUNT), SubpassAttachment::AS_ATOMIC_COUNTER, LIGHT_ID_COUNT)
            }, settings);

    settings = PassSettings::DefaultOutputRenderpassSettings();
    settings.ignoreApplication = false;
    settings.ignoreClear = true;
    settings.enable = { GL_BLEND };
    settings.depthFunc = GL_ALWAYS;
    settings.srcBlendFactor = GL_SRC_ALPHA;
    settings.dstBlendFactor = GL_ONE_MINUS_SRC_ALPHA;

    Renderpass& weightedBlendedBlendPass = pipelineWithShadowmap.pipeline.AddPass("Weighted blended transparency blend pass", settings);
    weightedBlendedBlendPass.fbo = 0;

    weightedBlendedBlendPass.AddSubpass("Blending pass", &weightedTransparencyBlendShader, SCREEN_QUAD, 
            {
                SubpassAttachment(&accumulator, SubpassAttachment::AS_TEXTURE, "accumulator"),
                SubpassAttachment(&revealage, SubpassAttachment::AS_TEXTURE, "revealage"),

                SubpassAttachment(pipelineWithShadowmap.shadowmap, SubpassAttachment::AS_TEXTURE, "shadow_map"),
                SubpassAttachment(&globalAttachments.GetAttachment(POINT_LIGHTS), SubpassAttachment::AS_SSBO, POINT_LIGHTS),
                SubpassAttachment(&globalAttachments.GetAttachment(LIGHT_TILE_DATA), SubpassAttachment::AS_SSBO, LIGHT_TILE_DATA),
                SubpassAttachment(&globalAttachments.GetAttachment(LIGHT_IDS), SubpassAttachment::AS_SSBO, LIGHT_IDS),
                SubpassAttachment(&globalAttachments.GetAttachment(LIGHT_ID_COUNT), SubpassAttachment::AS_ATOMIC_COUNTER, LIGHT_ID_COUNT)
            }, settings);

    if (configure)
    {
        assert(pipelineWithShadowmap.pipeline.ConfigureAttachments());
    }
    return pipelineWithShadowmap.pipeline;
}

RenderPipeline DepthPeelingPipeline(Renderpass& globalAttachments, ShaderPool& shaders)
{
    Shader& depthPeelingShader = shaders.GetShader(
        ShaderDescriptor(
            {
                ShaderDescriptor::File(SHADER_PATH "default.vert", ShaderDescriptor::VERTEX_SHADER),
                ShaderDescriptor::File(FRAG_COMMON_SHADER, ShaderDescriptor::FRAGMENT_SHADER),
                ShaderDescriptor::File(LIGHTING_COMMON_SHADER, ShaderDescriptor::FRAGMENT_SHADER),
                ShaderDescriptor::File(SHADER_PATH "depth_peeling.frag", ShaderDescriptor::FRAGMENT_SHADER)
            }, globalAttachments.DefineValues()));
    
    PipelineWithShadowmap pipelineWithShadowmap = UnconfiguredDeferredPipeline(globalAttachments, shaders);

#define DEPTH_PASS_COUNT 6
    Renderpass& depthPeelingPass = pipelineWithShadowmap.pipeline.AddPass("Depth peeling pass");
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
        Subpass& subpass = depthPeelingPass.AddSubpass(subpassName, &depthPeelingShader, (MeshTag)(TRANSPARENT | PARTICLE),
            { 
                SubpassAttachment(&depthPeelingPass.AddAttachment(RenderpassAttachment(outputBuffer, AttachmentFormat::FLOAT_4)), SubpassAttachment::AS_COLOR), // Not actually necessary, just for debug purposes -- could blend into output immediatelly
                SubpassAttachment(evenPeel ? &depthPeelingDepthA : &depthPeelingDepthB, SubpassAttachment::AS_DEPTH),
                SubpassAttachment(evenPeel ? &depthPeelingDepthB : &depthPeelingDepthA, SubpassAttachment::AS_TEXTURE, "greater_depth"),
                SubpassAttachment(pipelineWithShadowmap.shadowmap,                      SubpassAttachment::AS_TEXTURE, "shadow_map"),
                SubpassAttachment(&globalAttachments.GetAttachment(POINT_LIGHTS), SubpassAttachment::AS_SSBO, POINT_LIGHTS),
                SubpassAttachment(&globalAttachments.GetAttachment(LIGHT_TILE_DATA), SubpassAttachment::AS_SSBO, LIGHT_TILE_DATA),
                SubpassAttachment(&globalAttachments.GetAttachment(LIGHT_IDS), SubpassAttachment::AS_SSBO, LIGHT_IDS),
                SubpassAttachment(&globalAttachments.GetAttachment(LIGHT_ID_COUNT), SubpassAttachment::AS_ATOMIC_COUNTER, LIGHT_ID_COUNT)
            }, peelSettings);
    }

    pipelineWithShadowmap.pipeline.AddOutputPass(shaders);

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
        Renderpass& outputPass = pipelineWithShadowmap.pipeline.AddPass(name, settings);
        outputPass.fbo = 0;

        Subpass& subpass = outputPass.AddSubpass(subpassName, &shaders.GetShader(SCREEN_QUAD_TEXTURE_SHADER), SCREEN_QUAD,
                {
                // Default framebuffer already has a color attachment, no need to add another one
                SubpassAttachment(&depthPeelingPass.GetAttachment(layer), SubpassAttachment::AS_TEXTURE, "tex")
                }, settings);
    }

    assert(pipelineWithShadowmap.pipeline.ConfigureAttachments());
    return pipelineWithShadowmap.pipeline;
}

RenderPipeline DualDepthPeelingPipeline(Renderpass& globalAttachments, ShaderPool& shaders)
{
    Shader& dualDepthPeelingInitShader = shaders.GetShader(
        ShaderDescriptor(
            {
                ShaderDescriptor::File(SHADER_PATH "default.vert", ShaderDescriptor::VERTEX_SHADER),
                ShaderDescriptor::File(SHADER_PATH "dual_depth_peeling_init.frag", ShaderDescriptor::FRAGMENT_SHADER),
            }));
    Shader& dualDepthPeelingShader = shaders.GetShader(
        ShaderDescriptor(
            {
                ShaderDescriptor::File(SHADER_PATH "default.vert", ShaderDescriptor::VERTEX_SHADER),
                ShaderDescriptor::File(FRAG_COMMON_SHADER, ShaderDescriptor::FRAGMENT_SHADER),
                ShaderDescriptor::File(LIGHTING_COMMON_SHADER, ShaderDescriptor::FRAGMENT_SHADER),
                ShaderDescriptor::File(SHADER_PATH "dual_depth_peeling.frag", ShaderDescriptor::FRAGMENT_SHADER),
            }, globalAttachments.DefineValues()));
    Shader& dualDepthPeelingBackBlendingShader = shaders.GetShader(
        ShaderDescriptor(
            {
                ShaderDescriptor::File(SHADER_PATH "fallthrough.vert", ShaderDescriptor::VERTEX_SHADER),
                ShaderDescriptor::File(SHADER_PATH "dual_depth_peeling_back_blending.frag", ShaderDescriptor::FRAGMENT_SHADER),
            }));
    Shader& dualDepthPeelingCompositionShader = shaders.GetShader(
        ShaderDescriptor(
            {
                ShaderDescriptor::File(SHADER_PATH "fallthrough.vert", ShaderDescriptor::VERTEX_SHADER),
                ShaderDescriptor::File(SHADER_PATH "dual_depth_peeling_composition.frag", ShaderDescriptor::FRAGMENT_SHADER),
            }));

    PipelineWithShadowmap pipelineWithShadowmap = UnconfiguredDeferredPipeline(globalAttachments, shaders);

#define DUAL_DEPTH_PASS_COUNT 3
#define CLEAR_DEPTH -99999.f
#define MIN_DEPTH 0.f
#define MAX_DEPTH 1.f
    Renderpass& dualDepthPeelingPass = pipelineWithShadowmap.pipeline.AddPass("Dual depth peeling pass");
    RenderpassAttachment& minMaxDepthA = dualDepthPeelingPass.AddAttachment(RenderpassAttachment("min_max_depth_A", AttachmentFormat::FLOAT_2, AttachmentClearOpts(glm::vec4(MIN_DEPTH, MIN_DEPTH, 0.f, 0.f))));
    RenderpassAttachment& minMaxDepthB = dualDepthPeelingPass.AddAttachment(RenderpassAttachment("min_max_depth_B", AttachmentFormat::FLOAT_2, AttachmentClearOpts(glm::vec4(-MIN_DEPTH, MAX_DEPTH, 0.f, 0.f))));
    RenderpassAttachment& frontBlenderA = dualDepthPeelingPass.AddAttachment(RenderpassAttachment("front_blender_A", AttachmentFormat::FLOAT_4, AttachmentClearOpts(glm::vec4(0.f, 0.f, 0.f, 0.f))));
    RenderpassAttachment& frontBlenderB = dualDepthPeelingPass.AddAttachment(RenderpassAttachment("front_blender_B", AttachmentFormat::FLOAT_4, AttachmentClearOpts(glm::vec4(0.f, 0.f, 0.f, 0.f))));
    RenderpassAttachment& tempBackBlenderA = dualDepthPeelingPass.AddAttachment(RenderpassAttachment("temp_back_blender_A", AttachmentFormat::FLOAT_4, AttachmentClearOpts(glm::vec4(0.f, 0.f, 0.f, 0.f))));
    RenderpassAttachment& tempBackBlenderB = dualDepthPeelingPass.AddAttachment(RenderpassAttachment("temp_back_blender_B", AttachmentFormat::FLOAT_4, AttachmentClearOpts(glm::vec4(0.f, 0.f, 0.f, 0.f))));
    RenderpassAttachment& finalBackBlender = dualDepthPeelingPass.AddAttachment(RenderpassAttachment("back_blender", AttachmentFormat::FLOAT_4, AttachmentClearOpts(glm::vec4(0.f, 0.f, 0.f, 0.f))));

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
                SubpassAttachment(pipelineWithShadowmap.shadowmap,                  SubpassAttachment::AS_TEXTURE, "shadow_map"),
                SubpassAttachment(&globalAttachments.GetAttachment(POINT_LIGHTS), SubpassAttachment::AS_SSBO, POINT_LIGHTS),
                SubpassAttachment(&globalAttachments.GetAttachment(LIGHT_TILE_DATA), SubpassAttachment::AS_SSBO, LIGHT_TILE_DATA),
                SubpassAttachment(&globalAttachments.GetAttachment(LIGHT_IDS), SubpassAttachment::AS_SSBO, LIGHT_IDS),
                SubpassAttachment(&globalAttachments.GetAttachment(LIGHT_ID_COUNT), SubpassAttachment::AS_ATOMIC_COUNTER, LIGHT_ID_COUNT)
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

    pipelineWithShadowmap.pipeline.AddOutputPass(shaders);

    PassSettings settings = PassSettings::DefaultOutputRenderpassSettings();
    settings.ignoreApplication = false;
    settings.ignoreClear = true;
    settings.enable = { GL_BLEND };
    settings.blendEquation = GL_FUNC_ADD;
    settings.srcBlendFactor = GL_SRC_ALPHA;
    settings.dstBlendFactor = GL_ONE_MINUS_SRC_ALPHA;

    Renderpass& dualDepthPeelingCompositionPass = pipelineWithShadowmap.pipeline.AddPass("Dual depth peeling composition pass", settings);
    dualDepthPeelingCompositionPass.fbo = 0;

    dualDepthPeelingCompositionPass.AddSubpass("Dual depth peeling composition subpass", &dualDepthPeelingCompositionShader, SCREEN_QUAD,
            {
                    // Default framebuffer already has a color attachment, no need to add another one
                SubpassAttachment(DUAL_DEPTH_PASS_COUNT % 2 == 0 ? &frontBlenderA : &frontBlenderB, SubpassAttachment::AS_TEXTURE, "frontBlender"),
                SubpassAttachment(&finalBackBlender, SubpassAttachment::AS_TEXTURE, "backBlender"),
            }, settings);

    assert(pipelineWithShadowmap.pipeline.ConfigureAttachments());
    return pipelineWithShadowmap.pipeline;
}

PipelineWithShadowmap ABufferPPLLPipeline(Renderpass& globalAttachments, ShaderPool& shaders, const char* transparencyFragShaderFilepath,
        MeshTag meshTag = (MeshTag)(TRANSPARENT | PARTICLE), bool configure = true)
{
    PipelineWithShadowmap pipelineWithShadowmap = UnconfiguredDeferredPipeline(globalAttachments, shaders);

    // TODO: This is absolutely positively horrible. The pipeline should either be smart enough to figure out where the pass/subpass
    // can be inserted. Or there should at least be a better mechanism for inserting passes / subpasses.
    Renderpass* deferredPass = nullptr;
    Subpass* compositionSubpass = nullptr;
    int geometrySubpassIndex = -1;
    for (int i = 0; i < pipelineWithShadowmap.pipeline.passes.size() && deferredPass == nullptr; i++)
    {
        Renderpass* pass = pipelineWithShadowmap.pipeline.passes[i];
        if (pass == nullptr || strcmp(pass->name, DEFERRED_PASS) != 0)
        {
            continue;
        }

        deferredPass = pass;
        for (int j = 0; j < pass->subpasses.size() && (compositionSubpass == nullptr || geometrySubpassIndex == -1); j++)
        {
            Subpass* subpass = pass->subpasses[j];
            bool isGeometryPass = strcmp(subpass->name, GEOMETRY_SUBPASS) == 0;
            bool isCompositionPass = strcmp(subpass->name, COMPOSITION_LIGHTING_SUBPASS) == 0;
            if (subpass == nullptr || !(isGeometryPass || isCompositionPass))
            {
                continue;
            }

            if (isGeometryPass)
            {
                geometrySubpassIndex = j;
            }

            if (isCompositionPass)
            {
                compositionSubpass = subpass;
            }
        }
    }

    RenderpassAttachment& transparencyPPLLHeadIndices = deferredPass->AddAttachment(RenderpassAttachment("ppllHeads", AttachmentFormat::UINT_1));
    long fragmentDataSize = sizeof(glm::vec4) * 3;
    long maxTransparencyLayers = 8;
    long linkedListSize = 1920 * 1080 * maxTransparencyLayers * fragmentDataSize;
    LOG_WARN("", "linked list size: %d MB", linkedListSize / (1024 * 1024));
    RenderpassAttachment& transparencyPPLL = deferredPass->AddAttachment(RenderpassAttachment::SSBO("TransparentFragments", linkedListSize));
    RenderpassAttachment& transparentFragmentCount = deferredPass->AddAttachment(RenderpassAttachment::AtomicCounter("transparentFragmentCount"));

    Shader& transparentGeometryShader = shaders.GetShader(
        ShaderDescriptor(
            {
                ShaderDescriptor::File(SHADER_PATH "geometry_buffer.geom", ShaderDescriptor::GEOMETRY_SHADER),
                ShaderDescriptor::File(SHADER_PATH "geometry_buffer.vert", ShaderDescriptor::VERTEX_SHADER),
                ShaderDescriptor::File(SHADER_PATH "transparency_geometry_buffer.frag", ShaderDescriptor::FRAGMENT_SHADER)
            }));

    PassSettings transparentGeometryPassSettings = PassSettings::DefaultSubpassSettings();
    transparentGeometryPassSettings.ignoreApplication = false;
    transparentGeometryPassSettings.enable = { GL_DEPTH_TEST };
    transparentGeometryPassSettings.depthMask = GL_FALSE;
    deferredPass->InsertSubpass(geometrySubpassIndex + 1, "transparent geometry pass", &transparentGeometryShader, meshTag,
        {
            // NOTE: not sure, a bug or not - this uses the same attachment as the opaque pass since it uses the same FB.
            //SubpassAttachment(&deferredPass->GetAttachment("g_depth"), SubpassAttachment::AS_DEPTH),

            // TODO: allow picking all existing renderpass' (specific subpass') attachments and re-binding them as textures with the same names?
            SubpassAttachment(SubpassAttachment(&transparencyPPLLHeadIndices, SubpassAttachment::AS_IMAGE, "ppllHeads")),
            SubpassAttachment(SubpassAttachment(&transparencyPPLL, SubpassAttachment::AS_SSBO, "TransparentFragments")),
            SubpassAttachment(SubpassAttachment(&transparentFragmentCount, SubpassAttachment::AS_ATOMIC_COUNTER, "transparentFragmentCount")),
        }, transparentGeometryPassSettings);

    Shader& transparencySortingShader = shaders.GetShader(
        ShaderDescriptor(
            {
                ShaderDescriptor::File(SHADER_PATH "fallthrough.vert", ShaderDescriptor::VERTEX_SHADER),
                ShaderDescriptor::File(FRAG_COMMON_SHADER, ShaderDescriptor::FRAGMENT_SHADER),
                ShaderDescriptor::File(SHADER_PATH "ppll_depth_sort.frag", ShaderDescriptor::FRAGMENT_SHADER)
            }));
    deferredPass->InsertSubpass(geometrySubpassIndex + 2, "sorting subpass", &transparencySortingShader, SCREEN_QUAD,
        {
            SubpassAttachment(SubpassAttachment(&transparencyPPLLHeadIndices, SubpassAttachment::AS_IMAGE, "ppllHeads")),
            SubpassAttachment(SubpassAttachment(&transparencyPPLL, SubpassAttachment::AS_SSBO, "TransparentFragments")),
            SubpassAttachment(SubpassAttachment(&transparentFragmentCount, SubpassAttachment::AS_ATOMIC_COUNTER, "transparentFragmentCount")),
        }, PassSettings::DefaultSubpassSettings());

    Shader& deferredLightingWithTransparencyShader = shaders.GetShader(
        ShaderDescriptor(
            {
                ShaderDescriptor::File(SHADER_PATH "fallthrough.vert", ShaderDescriptor::VERTEX_SHADER),
                ShaderDescriptor::File(FRAG_COMMON_SHADER, ShaderDescriptor::FRAGMENT_SHADER),
                ShaderDescriptor::File(LIGHTING_COMMON_SHADER, ShaderDescriptor::FRAGMENT_SHADER),
                ShaderDescriptor::File(transparencyFragShaderFilepath, ShaderDescriptor::FRAGMENT_SHADER)
            }, globalAttachments.DefineValues(deferredPass->DefineValues())));
    // Replace the existing composition shader with a one respecting transparency
    compositionSubpass->shader = &deferredLightingWithTransparencyShader;
    // Add transparency data
    compositionSubpass->attachments.push_back(SubpassAttachment(&transparencyPPLLHeadIndices, SubpassAttachment::AS_IMAGE, "ppllHeads"));
    compositionSubpass->attachments.push_back(SubpassAttachment(&transparencyPPLL, SubpassAttachment::AS_SSBO, "TransparentFragments"));
    compositionSubpass->attachments.push_back(SubpassAttachment(&transparentFragmentCount, SubpassAttachment::AS_ATOMIC_COUNTER, "transparentFragmentCount"));

    pipelineWithShadowmap.pipeline.AddOutputPass(shaders);

    if (configure)
    {
        assert(pipelineWithShadowmap.pipeline.ConfigureAttachments());
    }
    return pipelineWithShadowmap;
}

RenderPipeline ABufferPPLLWeightedParticlesPipeline(Renderpass& globalAttachments, ShaderPool& shaders, const char* transparencyFragShaderFilepath)
{
    PipelineWithShadowmap basePipeline = ABufferPPLLPipeline(globalAttachments, shaders, transparencyFragShaderFilepath, TRANSPARENT, false);
    RenderPipeline pipelineWithWeightedParticles = WeightedBlendedTransparencyPipeline(basePipeline, globalAttachments, shaders, SHADER_PATH "weighted_depth_transparency.frag",
            PARTICLE, true);

    return pipelineWithWeightedParticles;
}

RenderPipeline NewMethod(Renderpass& globalAttachments, ShaderPool& shaders)
{
    PipelineWithShadowmap pipelineWithShadowmap = UnconfiguredDeferredPipeline(globalAttachments, shaders);

    // TODO: This is absolutely positively horrible. The pipeline should either be smart enough to figure out where the pass/subpass
    // can be inserted. Or there should at least be a better mechanism for inserting passes / subpasses.
    Renderpass* deferredPass = nullptr;
    Subpass* compositionSubpass = nullptr;
    int geometrySubpassIndex = -1;
    for (int i = 0; i < pipelineWithShadowmap.pipeline.passes.size() && deferredPass == nullptr; i++)
    {
        Renderpass* pass = pipelineWithShadowmap.pipeline.passes[i];
        if (pass == nullptr || strcmp(pass->name, DEFERRED_PASS) != 0)
        {
            continue;
        }

        deferredPass = pass;
        for (int j = 0; j < pass->subpasses.size() && (compositionSubpass == nullptr || geometrySubpassIndex == -1); j++)
        {
            Subpass* subpass = pass->subpasses[j];
            bool isGeometryPass = strcmp(subpass->name, GEOMETRY_SUBPASS) == 0;
            bool isCompositionPass = strcmp(subpass->name, COMPOSITION_LIGHTING_SUBPASS) == 0;
            if (subpass == nullptr || !(isGeometryPass || isCompositionPass))
            {
                continue;
            }

            if (isGeometryPass)
            {
                geometrySubpassIndex = j;
            }

            if (isCompositionPass)
            {
                compositionSubpass = subpass;
            }
        }
    }

    Shader& weightedTransparencyShader = shaders.GetShader(
        ShaderDescriptor(
            {
                ShaderDescriptor::File(SHADER_PATH "default.vert", ShaderDescriptor::VERTEX_SHADER),
                ShaderDescriptor::File(FRAG_COMMON_SHADER, ShaderDescriptor::FRAGMENT_SHADER),
                ShaderDescriptor::File(LIGHTING_COMMON_SHADER, ShaderDescriptor::FRAGMENT_SHADER),
                ShaderDescriptor::File(SHADER_PATH "weighted_depth_transparency_with_depth.frag", ShaderDescriptor::FRAGMENT_SHADER)
        }, globalAttachments.DefineValues()));
    Shader& weightedTransparencyBlendShader = shaders.GetShader(
        ShaderDescriptor(
            {
                ShaderDescriptor::File(SHADER_PATH "fallthrough.vert", ShaderDescriptor::VERTEX_SHADER),
                ShaderDescriptor::File(FRAG_COMMON_SHADER, ShaderDescriptor::FRAGMENT_SHADER),
                ShaderDescriptor::File(LIGHTING_COMMON_SHADER, ShaderDescriptor::FRAGMENT_SHADER),
                ShaderDescriptor::File(SHADER_PATH "weighted_transparency_blend.frag", ShaderDescriptor::FRAGMENT_SHADER)
        }, globalAttachments.DefineValues()));

    PassSettings settings = PassSettings::DefaultSubpassSettings();
    settings.ignoreApplication = false;
    settings.ignoreClear = true;
    settings.enable = { GL_BLEND, GL_DEPTH_TEST };
    settings.depthMask = GL_FALSE;
    settings.blendFactors.push_back({GL_ONE, GL_ONE});
    settings.blendFactors.push_back({GL_ZERO, GL_ONE_MINUS_SRC_COLOR});
    settings.blendEquation = GL_FUNC_ADD;

    RenderpassAttachment& accumulator = deferredPass->AddAttachment(RenderpassAttachment("accumulator", AttachmentFormat::FLOAT_4, AttachmentClearOpts(glm::vec4(0.f))));
    RenderpassAttachment& debugDepth = deferredPass->AddAttachment(RenderpassAttachment("d", AttachmentFormat::FLOAT_4, AttachmentClearOpts(glm::vec4(1.f))));
    RenderpassAttachment& revealage = deferredPass->AddAttachment(RenderpassAttachment("revealage", AttachmentFormat::FLOAT_1, AttachmentClearOpts(glm::vec4(1.f))));
    //RenderpassAttachment& transparencyDepth = deferredPass->AddAttachment(RenderpassAttachment("transpDepth", AttachmentFormat::UINT_1, AttachmentClearOpts(glm::vec4(1000000000000.f))));
    RenderpassAttachment& transparencyDepth = deferredPass->AddAttachment(RenderpassAttachment("transpDepth", AttachmentFormat::UINT_1, AttachmentClearOpts(glm::vec4(0.f))));

    deferredPass->InsertSubpass(geometrySubpassIndex + 1, "weighted blended transparency pass", &weightedTransparencyShader, PARTICLE, 
            {
                SubpassAttachment(&accumulator, SubpassAttachment::AS_COLOR),
                SubpassAttachment(&revealage, SubpassAttachment::AS_COLOR),
                SubpassAttachment(&debugDepth, SubpassAttachment::AS_COLOR),
                SubpassAttachment(&deferredPass->GetAttachment("g_depth"), SubpassAttachment::AS_DEPTH),
                SubpassAttachment(&transparencyDepth, SubpassAttachment::AS_IMAGE, "transparencyDepth"),

                SubpassAttachment(pipelineWithShadowmap.shadowmap, SubpassAttachment::AS_TEXTURE, "shadow_map"),
                SubpassAttachment(&globalAttachments.GetAttachment(POINT_LIGHTS), SubpassAttachment::AS_SSBO, POINT_LIGHTS),
                SubpassAttachment(&globalAttachments.GetAttachment(LIGHT_TILE_DATA), SubpassAttachment::AS_SSBO, LIGHT_TILE_DATA),
                SubpassAttachment(&globalAttachments.GetAttachment(LIGHT_IDS), SubpassAttachment::AS_SSBO, LIGHT_IDS),
                SubpassAttachment(&globalAttachments.GetAttachment(LIGHT_ID_COUNT), SubpassAttachment::AS_ATOMIC_COUNTER, LIGHT_ID_COUNT)
            }, settings);

    RenderpassAttachment& transparencyPPLLHeadIndices = deferredPass->AddAttachment(RenderpassAttachment("ppllHeads", AttachmentFormat::UINT_1));
    long fragmentDataSize = sizeof(glm::vec4) * 3;
    long maxTransparencyLayers = 8;
    long linkedListSize = 1920 * 1080 * maxTransparencyLayers * fragmentDataSize;
    LOG_WARN("", "linked list size: %d MB", linkedListSize / (1024 * 1024));
    RenderpassAttachment& transparencyPPLL = deferredPass->AddAttachment(RenderpassAttachment::SSBO("TransparentFragments", linkedListSize));
    RenderpassAttachment& transparentFragmentCount = deferredPass->AddAttachment(RenderpassAttachment::AtomicCounter("transparentFragmentCount"));

    Shader& transparentGeometryShader = shaders.GetShader(
        ShaderDescriptor(
            {
                ShaderDescriptor::File(SHADER_PATH "geometry_buffer.geom", ShaderDescriptor::GEOMETRY_SHADER),
                ShaderDescriptor::File(SHADER_PATH "geometry_buffer.vert", ShaderDescriptor::VERTEX_SHADER),
                ShaderDescriptor::File(SHADER_PATH "transparency_geometry_buffer.frag", ShaderDescriptor::FRAGMENT_SHADER)
            }));

    PassSettings transparentGeometryPassSettings = PassSettings::DefaultSubpassSettings();
    transparentGeometryPassSettings.ignoreApplication = false;
    transparentGeometryPassSettings.enable = { GL_DEPTH_TEST };
    transparentGeometryPassSettings.depthMask = GL_FALSE;
    deferredPass->InsertSubpass(geometrySubpassIndex + 2, "transparent geometry pass", &transparentGeometryShader, TRANSPARENT,
        {
            // NOTE: not sure, a bug or not - this uses the same attachment as the opaque pass since it uses the same FB.
            //SubpassAttachment(&deferredPass->GetAttachment("g_depth"), SubpassAttachment::AS_DEPTH),

            // TODO: allow picking all existing renderpass' (specific subpass') attachments and re-binding them as textures with the same names?
            SubpassAttachment(SubpassAttachment(&transparencyPPLLHeadIndices, SubpassAttachment::AS_IMAGE, "ppllHeads")),
            SubpassAttachment(SubpassAttachment(&transparencyPPLL, SubpassAttachment::AS_SSBO, "TransparentFragments")),
            SubpassAttachment(SubpassAttachment(&transparentFragmentCount, SubpassAttachment::AS_ATOMIC_COUNTER, "transparentFragmentCount")),
        }, transparentGeometryPassSettings);

    Shader& transparencySortingShader = shaders.GetShader(
        ShaderDescriptor(
            {
                ShaderDescriptor::File(SHADER_PATH "fallthrough.vert", ShaderDescriptor::VERTEX_SHADER),
                ShaderDescriptor::File(FRAG_COMMON_SHADER, ShaderDescriptor::FRAGMENT_SHADER),
                ShaderDescriptor::File(SHADER_PATH "ppll_depth_sort.frag", ShaderDescriptor::FRAGMENT_SHADER)
            }));
    deferredPass->InsertSubpass(geometrySubpassIndex + 3, "sorting subpass", &transparencySortingShader, SCREEN_QUAD,
        {
            SubpassAttachment(SubpassAttachment(&transparencyPPLLHeadIndices, SubpassAttachment::AS_IMAGE, "ppllHeads")),
            SubpassAttachment(SubpassAttachment(&transparencyPPLL, SubpassAttachment::AS_SSBO, "TransparentFragments")),
            SubpassAttachment(SubpassAttachment(&transparentFragmentCount, SubpassAttachment::AS_ATOMIC_COUNTER, "transparentFragmentCount")),
        }, PassSettings::DefaultSubpassSettings());

    Shader& deferredLightingWithTransparencyShader = shaders.GetShader(
        ShaderDescriptor(
            {
                ShaderDescriptor::File(SHADER_PATH "fallthrough.vert", ShaderDescriptor::VERTEX_SHADER),
                ShaderDescriptor::File(FRAG_COMMON_SHADER, ShaderDescriptor::FRAGMENT_SHADER),
                ShaderDescriptor::File(LIGHTING_COMMON_SHADER, ShaderDescriptor::FRAGMENT_SHADER),
                ShaderDescriptor::File(SHADER_PATH "deferred_lighting_with_transparency_weighted_blended.frag", ShaderDescriptor::FRAGMENT_SHADER)
            }, globalAttachments.DefineValues(deferredPass->DefineValues())));
    // Replace the existing composition shader with a one respecting transparency
    compositionSubpass->shader = &deferredLightingWithTransparencyShader;
    // Add transparency data
    compositionSubpass->attachments.push_back(SubpassAttachment(&accumulator, SubpassAttachment::AS_TEXTURE, "accumulator"));
    compositionSubpass->attachments.push_back(SubpassAttachment(&revealage, SubpassAttachment::AS_TEXTURE, "revealage"));
    compositionSubpass->attachments.push_back(SubpassAttachment(&transparencyDepth, SubpassAttachment::AS_IMAGE, "transparencyDepth"));
    compositionSubpass->attachments.push_back(SubpassAttachment(&transparencyPPLLHeadIndices, SubpassAttachment::AS_IMAGE, "ppllHeads"));
    compositionSubpass->attachments.push_back(SubpassAttachment(&transparencyPPLL, SubpassAttachment::AS_SSBO, "TransparentFragments"));
    compositionSubpass->attachments.push_back(SubpassAttachment(&transparentFragmentCount, SubpassAttachment::AS_ATOMIC_COUNTER, "transparentFragmentCount"));

    pipelineWithShadowmap.pipeline.AddOutputPass(shaders);

    assert(pipelineWithShadowmap.pipeline.ConfigureAttachments());
    return pipelineWithShadowmap.pipeline;
}

std::vector<NamedPipeline> TestPipelines(Renderpass& globalAttachments, ShaderPool& shaders)
{
    return 
        {
            { "No transparency", NoTransparencyPipeline(globalAttachments, shaders) },
            { "Unsorted forward transparency", UnsortedForwardTransparencyPipeline(globalAttachments, shaders) },
            { "Sorted forward transparency", UnsortedForwardTransparencyPipeline(globalAttachments, shaders) },
            //{ "Weighted blended transparency (Meshkin)", WeightedBlendedTransparencyPipeline(globalAttachments, shaders, SHADER_PATH "meshkin_weighted_transparency.frag") },
            //{ "Weighted blended transparency (Bavoil & Myers)", WeightedBlendedTransparencyPipeline(globalAttachments, shaders, SHADER_PATH "bavoil_myers_weighted_transparency.frag") },
            { "Weighted blended transparency (McGuire & Bavoil - depth weights)", WeightedBlendedTransparencyPipeline(UnconfiguredDeferredPipeline(globalAttachments, shaders),
                    globalAttachments, shaders, SHADER_PATH "weighted_depth_transparency.frag") },
            { "Depth peeling", DepthPeelingPipeline(globalAttachments, shaders) },
            { "Dual depth peeling", DualDepthPeelingPipeline(globalAttachments, shaders) },
            { "A-Buffer OIT: PPLL (simple)", ABufferPPLLPipeline(globalAttachments, shaders, SHADER_PATH "deferred_lighting_with_transparency.frag").pipeline },
            { "Naive A-Buffer OIT: PPLL (simple) + weighted blended particles", ABufferPPLLWeightedParticlesPipeline(globalAttachments, shaders,
                    SHADER_PATH "deferred_lighting_with_transparency.frag") },
            { "Corrected A-Buffer OIT: PPLL (simple) + weighted blended particles", NewMethod(globalAttachments, shaders) },
            { "A-Buffer OIT: PPLL (volumetric)", ABufferPPLLPipeline(globalAttachments, shaders, SHADER_PATH "deferred_lighting_with_volumetric_transparency.frag").pipeline },
            { "Naive A-Buffer OIT: PPLL (volumetric) + weighted blended particles", ABufferPPLLWeightedParticlesPipeline(globalAttachments, shaders,
                    SHADER_PATH "deferred_lighting_with_volumetric_transparency.frag") },
        };
}
