#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <vector>
#include <algorithm>

#include "GLFW/glfw3.h"
#include "glm/gtx/transform.hpp"
#include "glm/gtx/string_cast.hpp"

#include "deferred_render_test.h"
#include "shader.h"

#define GL_ERROR_GUARD(msg) do {                          \
        GLenum err = glGetError();                        \
        if (err != GL_NO_ERROR)                           \
        {                                                 \
            printf("OpenGL error: %04x. %s\n", err, msg); \
            assert(false);                                \
        }                                                 \
    } while(0);

unsigned int CreateQuad(glm::vec2 ndcTopLeft, glm::vec2 ndcSize)
{
    float *quadVertices = (float*)malloc(sizeof(float) * 5 * 4);
    float vertices[] = {
        // pos                                                   // uvs
        ndcTopLeft.x            , ndcTopLeft.y            , 0.f, 0.f, 1.f,
        ndcTopLeft.x            , ndcTopLeft.y - ndcSize.y, 0.f, 0.f, 0.f,
        ndcTopLeft.x + ndcSize.x, ndcTopLeft.y            , 0.f, 1.f, 1.f,
        ndcTopLeft.x + ndcSize.x, ndcTopLeft.y - ndcSize.y, 0.f, 1.f, 0.f,
    };

    memcpy(quadVertices, &vertices, sizeof(float) * 5 * 4);

    unsigned int quadVAO;
    glGenVertexArrays(1, &quadVAO);
    unsigned int quadVBO;
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 4 * 5, quadVertices, GL_STATIC_DRAW);
    glEnableVertexArrayAttrib(quadVAO, 0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexArrayAttrib(quadVAO, 1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

    return quadVAO;
}

#define SCR_WIDTH 1920
#define SCR_HEIGHT 1080
DeferredTest::DeferredTest()
{
    pointShadowPass = Shader("../src/shaders/point_shadow_cubemap.vert", "../src/shaders/point_shadow_cubemap.geom", "../src/shaders/point_shadow_cubemap.frag");
    if (!pointShadowPass.compilationSucceeded)
        printf("FAILED CREATING SHADER PROGRAM (point shadow pass):\n%s\n", pointShadowPass.compilationErrorMsg);
    else
        printf("Shader compilation succeeded\n");

    directionalShadowPass = Shader("../src/shaders/directional_shadow.vert", "../src/shaders/directional_shadow.frag");
    if (!directionalShadowPass.compilationSucceeded)
        printf("FAILED CREATING SHADER PROGRAM (directional shadow pass):\n%s\n", directionalShadowPass.compilationErrorMsg);
    else
        printf("Shader compilation succeeded\n");

    geometryPass = Shader("../src/shaders/g_buf.vert", "../src/shaders/g_buf.frag");
    if (!geometryPass.compilationSucceeded)
        printf("FAILED CREATING SHADER PROGRAM (geometry pass):\n%s\n", geometryPass.compilationErrorMsg);
    else
        printf("Shader compilation succeeded\n");

    lightingPass = Shader("../src/shaders/deferred_lighting.vert", "../src/shaders/deferred_lighting.frag");
    if (!lightingPass.compilationSucceeded)
        printf("FAILED CREATING SHADER PROGRAM (lighting pass):\n%s\n", lightingPass.compilationErrorMsg);
    else
        printf("Shader compilation succeeded\n");

    quadPass = Shader("../src/shaders/texture.vert", "../src/shaders/texture.frag");
    if (!quadPass.compilationSucceeded)
        printf("FAILED CREATING SHADER PROGRAM (quad pass):\n%s\n", quadPass.compilationErrorMsg);
    else
        printf("Shader compilation succeeded\n");

    forwardTransparencyPass = Shader("../src/shaders/forward_transparency.vert", "../src/shaders/forward_transparency.frag");
    if (!forwardTransparencyPass.compilationSucceeded)
        printf("FAILED CREATING SHADER PROGRAM (forward transparency pass):\n%s\n", forwardTransparencyPass.compilationErrorMsg);
    else
        printf("Shader compilation succeeded\n");

    assert(pointShadowPass.compilationSucceeded && directionalShadowPass.compilationSucceeded && geometryPass.compilationSucceeded && 
            lightingPass.compilationSucceeded && quadPass.compilationSucceeded && forwardTransparencyPass.compilationSucceeded);

    // Point shadow fbo
    {
        glGenFramebuffers(1, &pointShadowFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, pointShadowFBO);

        glGenTextures(1, &shadowDepthCubemap);
        glBindTexture(GL_TEXTURE_CUBE_MAP, shadowDepthCubemap);

#define SHADOW_WIDTH 1024.f
#define SHADOW_HEIGHT 1024.f
        for (int i = 0; i < 6; i++)
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, shadowDepthCubemap, 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    // Directional shadow fbo
    {
        glGenFramebuffers(1, &directionalShadowFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, directionalShadowFBO);

        glGenTextures(1, &shadowDepth);
        glBindTexture(GL_TEXTURE_2D, shadowDepth);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SCR_WIDTH, SCR_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowDepth, 0);

        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    glGenFramebuffers(1, &gBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);

    // depth & stencil buffers
    glGenRenderbuffers(1, &depthStencil);
    glBindRenderbuffer(GL_RENDERBUFFER, depthStencil);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, SCR_WIDTH, SCR_HEIGHT);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthStencil);

    // - position color buffer
    glGenTextures(1, &gPosition);
    glBindTexture(GL_TEXTURE_2D, gPosition);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gPosition, 0);
    posTex = new Texture(gPosition, glm::vec2(SCR_WIDTH, SCR_HEIGHT));

    // - normal color buffer
    glGenTextures(1, &gNormal);
    glBindTexture(GL_TEXTURE_2D, gNormal);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, gNormal, 0);
    normalTex = new Texture(gNormal, glm::vec2(SCR_WIDTH, SCR_HEIGHT));

    // - color + specular color buffer
    glGenTextures(1, &gAlbedo);
    glBindTexture(GL_TEXTURE_2D, gAlbedo);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, gAlbedo, 0);
    albedoTex = new Texture(gAlbedo, glm::vec2(SCR_WIDTH, SCR_HEIGHT));

    glGenTextures(1, &gSpec);
    glBindTexture(GL_TEXTURE_2D, gSpec);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, gSpec, 0);
    specTex = new Texture(gSpec, glm::vec2(SCR_WIDTH, SCR_HEIGHT));

    // - tell OpenGL which color attachments we'll use (of this framebuffer) for rendering
    unsigned int attachments[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3 };
    glDrawBuffers(4, attachments);

    glGenFramebuffers(1, &finalFb);
    glBindFramebuffer(GL_FRAMEBUFFER, finalFb);

    glGenTextures(1, &finalTexId);
    glBindTexture(GL_TEXTURE_2D, finalTexId);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, finalTexId, 0);
    finalTex = new Texture(finalTexId, glm::vec2(SCR_WIDTH, SCR_HEIGHT));
    unsigned int drawBuffers[] = { GL_COLOR_ATTACHMENT0 };
    glDrawBuffers(1, drawBuffers);

    mainQuadVAO = CreateQuad(glm::vec2(-1.f, 1.f), glm::vec2(2, 2));
    float size = 2.f / 5.f;
    for(int i = 0; i < 5; i++)
        quadVAOs.push_back(CreateQuad(glm::vec2(-1.f, 1.f - (size * i)), glm::vec2(size, size)));

    //groundModel = Model::DoubleSidedQuad(glm::vec3(1.f, 1.f, 1.f));
    //lightModel = Model::DoubleSidedQuad(glm::vec3(1.f, 1.f, 1.f));
    lightModel = new Model("../assets/suzanne.obj",
            std::vector<std::pair<std::string, std::string>> {
                std::make_pair("tex_diffuse", "textures/default.jpeg"),
                std::make_pair("tex_normal", "textures/default.jpeg"),
            });
    //model = new Model("../assets/suzanne.obj",
    //        std::vector<std::pair<std::string, std::string>> {
    //            std::make_pair("tex_diffuse", "textures/default.jpeg"),
    //            std::make_pair("tex_normal", "textures/default.jpeg"),
    //            //std::make_pair("tex_specular", "textures/default.jpeg")
    //        });
    //model = new Model("../assets/backpack/backpack.obj",
    //        std::vector<std::pair<std::string, std::string>> {
    //            std::make_pair("tex_diffuse", "../assets/backpack/diffuse.jpg"),
    //            std::make_pair("tex_normal", "../assets/backpack/normal.png"),
    //            std::make_pair("tex_specular", "../assets/backpack/specular.jpg")
    //        });
    model = new Model("../assets/sponza/sponza.obj");
    //        std::vector<std::pair<std::string, std::string>> {
    //            std::make_pair("tex_diffuse", "../assets/textures/default.jpeg"),
    //            std::make_pair("tex_normal", "../assets/textures/default.jpeg"),
    //            std::make_pair("tex_specular", "../assets/textures/default.jpeg")
    //        });
    //model = new Model("../assets/dragon.obj");

    transparentModel = new Model("../assets/suzanne.obj",
            std::vector<std::pair<std::string, std::string>> {
                std::make_pair("tex_diffuse", "textures/default.jpeg"),
                std::make_pair("tex_normal", "textures/default.jpeg"),
            });

    //transparentModelTransform = Transform(glm::vec3(-200.f, 100.f, 0.f), glm::quat(glm::vec3(0.f, 90.f / 180.f * glm::pi<float>(), 0.f)), glm::vec3(75.f));

    transparentModelData[0].opacity = 0.125f;
    transparentModelData[1].opacity = 0.25f;
    transparentModelData[2].opacity = 0.5f;

    transparentModelData[0].color = glm::vec3(1.f, 0.f, 0.f);
    transparentModelData[1].color = glm::vec3(0.f, 1.f, 0.f);
    transparentModelData[2].color = glm::vec3(0.f, 0.f, 1.f);

    transparentModelData[0].transform = Transform(glm::vec3(-200.f, 100.f, -25.f), glm::quat(glm::vec3(0.f, 90.f / 180.f * glm::pi<float>(), 0.f)), glm::vec3(75.f));
    transparentModelData[1].transform = Transform(glm::vec3(-200.f, 100.f, -25.f), glm::quat(glm::vec3(0.f, 90.f / 180.f * glm::pi<float>(), 0.f)), glm::vec3(75.f));
    transparentModelData[2].transform = Transform(glm::vec3(-200.f, 100.f, -25.f), glm::quat(glm::vec3(0.f, 90.f / 180.f * glm::pi<float>(), 0.f)), glm::vec3(75.f));

    groundTransform = Transform(glm::vec3(0.f, -10.f, 0.f), glm::quat(glm::vec3(90.f * glm::pi<float>() / 180.f, 0.f, 0.f)), glm::vec3(50.f, 50.f, 50.f));
    pointLightTransform = Transform(glm::vec3(0.f, 10.f, 0.f), glm::quat(glm::vec3(0.f, 0.f, 0.f)), glm::vec3(10.f, 10.f, 10.f));
    //directionalLightTransform = Transform(glm::vec3(1059.f, 1710.f, -225.f), glm::quat(glm::vec3(0.79f, -1.328f, 0.f)), glm::vec3(1.f, 1.f, 1.f));
    //directionalLightTransform = Transform(glm::vec3(2437.f, 2277.f, 462.f), glm::quat(glm::vec3(0.8f, -1.264f, 0.f)), glm::vec3(1.f, 1.f, 1.f));
    directionalLightTransform = Transform(glm::vec3(963.14f, 1505.95f, -226.6f), glm::quat(glm::vec3(0.6708f, -1.252, 0.f)), glm::vec3(1.f, 1.f, 1.f));

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void DeferredTest::Render()
{
    if (cameraOrbit)
    {
        camera.transform.rot = glm::quatLookAt(glm::normalize(camera.transform.pos), glm::vec3(0.f, 1.f, 0.f));
    }

    static std::vector<Transform> transforms;
    if (transforms.size() == 0)
    {
        transforms.push_back(Transform(glm::vec3(0.f, 0.f, 0.f), glm::quat(glm::vec3(0.f, 0.f, 0.f)), glm::vec3(1.f, 1.f, 1.f)));
        //for (int i = 0; i < 3; i++)
        //{
        //    transforms.push_back(Transform(glm::vec3(-5.f + 5.f*i, 0.f, 0.f)));
        //    transforms.push_back(Transform(glm::vec3(0.f, 0.f, -5.f + 5.f*i)));
        //}
    }

    if (pointShadows)
    {
        glViewport(0, 0, SHADOW_WIDTH, SHADOW_WIDTH);
        glBindFramebuffer(GL_FRAMEBUFFER, pointShadowFBO);
        glClear(GL_DEPTH_BUFFER_BIT);
        glCullFace(GL_FRONT); // Resolve Peter Panning

        float aspect = SHADOW_WIDTH/SHADOW_HEIGHT;
        float near = 1.0f;
        float far = 100000.0f;
        glm::mat4 shadowProjection = glm::perspective(glm::radians(90.0f), aspect, near, far); 

        std::vector<glm::mat4> shadowTransforms;
        shadowTransforms.push_back(shadowProjection * glm::lookAt(pointLightTransform.pos, pointLightTransform.pos + glm::vec3( 1.0, 0.0, 0.0), glm::vec3(0.0,-1.0, 0.0)));
        shadowTransforms.push_back(shadowProjection * glm::lookAt(pointLightTransform.pos, pointLightTransform.pos + glm::vec3(-1.0, 0.0, 0.0), glm::vec3(0.0,-1.0, 0.0)));
        shadowTransforms.push_back(shadowProjection * glm::lookAt(pointLightTransform.pos, pointLightTransform.pos + glm::vec3( 0.0, 1.0, 0.0), glm::vec3(0.0, 0.0, 1.0)));
        shadowTransforms.push_back(shadowProjection * glm::lookAt(pointLightTransform.pos, pointLightTransform.pos + glm::vec3( 0.0,-1.0, 0.0), glm::vec3(0.0, 0.0,-1.0)));
        shadowTransforms.push_back(shadowProjection * glm::lookAt(pointLightTransform.pos, pointLightTransform.pos + glm::vec3( 0.0, 0.0, 1.0), glm::vec3(0.0,-1.0, 0.0)));
        shadowTransforms.push_back(shadowProjection * glm::lookAt(pointLightTransform.pos, pointLightTransform.pos + glm::vec3( 0.0, 0.0,-1.0), glm::vec3(0.0,-1.0, 0.0)));

        pointShadowPass.Use();
        for (int i = 0; i < 6; i++)
        {
            char uniformName[11];
            sprintf(uniformName, "shadowVP[%d]", i);
            pointShadowPass.SetUniform(uniformName, shadowTransforms[i]);
        }
        pointShadowPass.SetUniform("lightPos", pointLightTransform.pos);
        pointShadowPass.SetUniform("farPlane", far);

        for (int i = 0; i < transforms.size(); i++)
        {
            pointShadowPass.SetUniform("model", transforms[i].Model());
            model->Render(pointShadowPass);
        }

        glCullFace(GL_BACK);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);

    if (directionalShadows)
    {
        //glViewport(0, 0, SHADOW_WIDTH, SHADOW_WIDTH);
        glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
        glBindFramebuffer(GL_FRAMEBUFFER, directionalShadowFBO);
        glClear(GL_DEPTH_BUFFER_BIT);
        glCullFace(GL_FRONT); // Resolve Peter Panning

        //glm::mat4 projection = glm::ortho(0.f, 1920.f, 1080.f, 0.f, -1.f, far);
        //glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspect, near, far); 
        glm::mat4 projection = camera.projection;
        glm::mat4 view = glm::lookAt(directionalLightTransform.pos, directionalLightTransform.pos + directionalLightTransform.Forward(), directionalLightTransform.Up());

        directionalShadowPass.Use();
        directionalShadowPass.SetUniform("projection", projection);
        directionalShadowPass.SetUniform("view", view);

        directionalShadowPass.SetUniform("lightPos", directionalLightTransform.pos);
        directionalShadowPass.SetUniform("farPlane", 100000.f);

        for (int i = 0; i < transforms.size(); i++)
        {
            directionalShadowPass.SetUniform("model", transforms[i].Model());
            model->Render(directionalShadowPass);
        }

        glCullFace(GL_BACK);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    // Deferred render pass
    glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glm::mat4 projection = camera.projection;
    glm::mat4 view = camera.View();

    geometryPass.Use();
    geometryPass.SetUniform("projection", projection);
    geometryPass.SetUniform("view", view);

    if (glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS)
    {
        geometryPass.SetUniform("showModelNormals", true);
        geometryPass.SetUniform("showNonTBNNormals", false);
    }
    if (glfwGetKey(window, GLFW_KEY_N) == GLFW_PRESS)
    {
        geometryPass.SetUniform("showModelNormals", false);
        geometryPass.SetUniform("showNonTBNNormals", true);
    }
    if (glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS)
    {
        geometryPass.SetUniform("showModelNormals", false);
        geometryPass.SetUniform("showNonTBNNormals", false);
    }
    if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS)
    {
        directionalLightTransform = camera.transform;
    }

    // Render model
    for (int i = 0; i < transforms.size(); i++)
    {
        geometryPass.SetUniform("model", transforms[i].Model());
        model->Render(geometryPass);
    }
    /*
    // render ground
    geometryPass.SetUniform("model", groundTransform.Model());
    groundModel->Render(geometryPass);
    */
    // render light node
    geometryPass.SetUniform("model", pointLightTransform.Model());
    lightModel->Render(geometryPass);

    glActiveTexture(GL_TEXTURE0);
    // lighting & composition pass
    glBindFramebuffer(GL_FRAMEBUFFER, finalFb);
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT);

    lightingPass.Use();
    lightingPass.SetUniform("pointLightPos", pointLightTransform.pos);
    lightingPass.SetUniform("lightColor", glm::vec3(1.f, 1.f, 1.f));
    lightingPass.SetUniform("cameraPos", camera.transform.pos);

    lightingPass.SetUniform("directionalBias", directionalBias);
    lightingPass.SetUniform("directionalAngleBias", directionalAngleBias);

    lightingPass.SetUniform("pointBias", pointBias);
    lightingPass.SetUniform("pointAngleBias", pointAngleBias);
    
    lightingPass.SetUniform("gamma", gamma);

    lightingPass.SetUniform("directionalLightPos", directionalLightTransform.pos);
    glm::mat4 proj = camera.projection * glm::lookAt(directionalLightTransform.pos, directionalLightTransform.pos + directionalLightTransform.Forward(), directionalLightTransform.Up());

    lightingPass.SetUniform("directionalLightViewProjection", proj);

    // bind gbuf textures
    glActiveTexture(GL_TEXTURE0);
    lightingPass.SetUniform("tex_pos", 0);
    posTex->Bind();

    glActiveTexture(GL_TEXTURE1);
    lightingPass.SetUniform("tex_diffuse", 1);
    albedoTex->Bind();

    glActiveTexture(GL_TEXTURE2);
    lightingPass.SetUniform("tex_normal", 2);
    normalTex->Bind();

    glActiveTexture(GL_TEXTURE3);
    lightingPass.SetUniform("tex_specular", 3);
    specTex->Bind();

    lightingPass.SetUniform("using_shadow_cubemap", pointShadows);
    {
        glActiveTexture(GL_TEXTURE4);
        lightingPass.SetUniform("shadow_cubemap", 4);
        glBindTexture(GL_TEXTURE_CUBE_MAP, shadowDepthCubemap);
    }

    lightingPass.SetUniform("using_shadow_map", directionalShadows);
    {
        glActiveTexture(GL_TEXTURE5);
        lightingPass.SetUniform("shadow_map", 5);
        glBindTexture(GL_TEXTURE_2D, shadowDepth);
    }

    glBindVertexArray(mainQuadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    static float offset = 0.f;
    pointLightTransform.pos = glm::vec3(-50.f + glm::sin(offset) * 100.f, 600.f, 200.f + glm::cos(offset) * 100.f);
    offset += 0.015f;

    // determine which buffer to show
    std::vector<Texture*> texs = { albedoTex, posTex, normalTex, specTex, finalTex };
    static int texIndex = texs.size() - 1;
    for(int i = 0; i < texs.size(); i++)
    {
        if (glfwGetKey(window, GLFW_KEY_1 + i) == GLFW_PRESS)
        {
            texIndex = i;
            break;
        }
    }

    // final quad render
    glActiveTexture(GL_TEXTURE0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClearColor(0.2f, 0.2f, 0.3f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Blit depth and forward render transparent objects
    glBindFramebuffer(GL_READ_FRAMEBUFFER, gBuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, SCR_WIDTH, SCR_HEIGHT, 0, 0, SCR_WIDTH, SCR_HEIGHT, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glDepthMask(GL_FALSE);
    quadPass.Use();
    texs[texIndex]->Bind();
    glBindVertexArray(mainQuadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    //glDepthMask(GL_TRUE);

    // forward render transparent objects
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    forwardTransparencyPass.Use();
    forwardTransparencyPass.SetUniform("projection", projection);
    forwardTransparencyPass.SetUniform("view", view);

    glDisable(GL_CULL_FACE);
    transparentModelData[1].transform.pos = transparentModelData[0].transform.pos + glm::vec3(glm::sin(offset) * 200.f, 0.f, glm::cos(offset) * 200.f);
    transparentModelData[2].transform.pos = transparentModelData[0].transform.pos - glm::vec3(glm::sin(offset) * 200.f, 0.f, glm::cos(offset) * 200.f);
    std::vector<int> indices = { 0, 1, 2 };
    std::sort(indices.begin(), indices.end(), [&](int a, int b) { return glm::distance(camera.transform.pos, transparentModelData[a].transform.pos) > glm::distance(camera.transform.pos, transparentModelData[b].transform.pos); });

    for (int i = 0; i < 3; i++)
    {
        forwardTransparencyPass.SetUniform("model", transparentModelData[indices[i]].transform.Model());
        forwardTransparencyPass.SetUniform("color", transparentModelData[indices[i]].color);
        forwardTransparencyPass.SetUniform("opacity", transparentModelData[indices[i]].opacity);
        transparentModel->Render(forwardTransparencyPass);
    }
    glEnable(GL_CULL_FACE);

    glDisable(GL_BLEND);


    glDepthMask(GL_TRUE);
    quadPass.Use();
    // render side quads
    for(int i = 0; i < texs.size(); i++)
    {
        texs[i]->Bind();
        glBindVertexArray(quadVAOs[i]);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    /*
    // wtf why is this not in reverse? maybe z testing?
    // main view
    texs[texIndex]->Bind();
    glBindVertexArray(mainQuadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    */

    // postfx
    glBindVertexArray(0);
}
