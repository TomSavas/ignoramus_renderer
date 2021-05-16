#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "GLFW/glfw3.h"
#include "glm/gtx/transform.hpp"

#include "deferred_render_test.h"
#include "shader.h"

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
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

    return quadVAO;
}

#define SCR_WIDTH 1920
#define SCR_HEIGHT 1080
DeferredTest::DeferredTest()
{
    geometryPass = Shader("../src/shaders/g_buf.vert", "../src/shaders/g_buf.frag");
    if (!geometryPass.compilationSucceeded)
        printf("FAILED CREATING SHADER PROGRAM (geometry pass):\n%s", geometryPass.compilationErrorMsg);
    else
        printf("Shader compilation succeeded\n");

    lightingPass = Shader("../src/shaders/deferred_lighting.vert", "../src/shaders/deferred_lighting.frag");
    if (!lightingPass.compilationSucceeded)
        printf("FAILED CREATING SHADER PROGRAM (lighting pass):\n%s", lightingPass.compilationErrorMsg);
    else
        printf("Shader compilation succeeded\n");

    quadPass = Shader("../src/shaders/texture.vert", "../src/shaders/texture.frag");
    if (!quadPass.compilationSucceeded)
        printf("FAILED CREATING SHADER PROGRAM (quad pass):\n%s", quadPass.compilationErrorMsg);
    else
        printf("Shader compilation succeeded\n");

    if (!geometryPass.compilationSucceeded || !lightingPass.compilationSucceeded || !quadPass.compilationSucceeded)
    {
        assert(false);
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
    model = new Model("../assets/suzanne.obj",
            std::vector<std::pair<std::string, std::string>> { 
                std::make_pair("tex_diffuse", "textures/default.jpeg"),
                std::make_pair("tex_normal", "textures/default.jpeg"),
                //std::make_pair("tex_specular", "textures/default.jpeg") 
            });
    //model = new Model("../assets/backpack/backpack.obj",
    //        std::vector<std::pair<std::string, std::string>> { 
    //            std::make_pair("tex_diffuse", "../assets/backpack/diffuse.jpg"),
    //            std::make_pair("tex_normal", "../assets/backpack/normal.png"),
    //            std::make_pair("tex_specular", "../assets/backpack/specular.jpg") 
    //        });
    //model = new Model("../assets/sponza/sponza.obj");
    //        std::vector<std::pair<std::string, std::string>> { 
    //            std::make_pair("tex_diffuse", "../assets/textures/default.jpeg"),
    //            std::make_pair("tex_normal", "../assets/textures/default.jpeg"),
    //            std::make_pair("tex_specular", "../assets/textures/default.jpeg") 
    //        });
    //model = new Model("../assets/dragon.obj");

    groundTransform = Transform(glm::vec3(0.f, -10.f, 0.f), glm::quat(glm::vec3(90.f * glm::pi<float>() / 180.f, 0.f, 0.f)), glm::vec3(50.f, 50.f, 50.f));
    lightTransform = Transform(glm::vec3(0.f, 10.f, 0.f), glm::quat(glm::vec3(0.f, 0.f, 0.f)), glm::vec3(0.1f, 0.1f, 0.1f));

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void DeferredTest::Render()
{
    GLenum err;
    while((err = glGetError()) != GL_NO_ERROR)
        printf("opengl error: %04x\n", err);

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
    // render light node
    geometryPass.SetUniform("model", lightTransform.Model());
    lightModel->Render(geometryPass);
    */

    glActiveTexture(GL_TEXTURE0);
    // lighting & composition pass
    glBindFramebuffer(GL_FRAMEBUFFER, finalFb);
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT);

    lightingPass.Use();
    lightingPass.SetUniform("lightPos", lightTransform.pos);
    lightingPass.SetUniform("lightColor", glm::vec3(1.f, 1.f, 1.f));
    lightingPass.SetUniform("cameraPos", camera.transform.pos);

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

    glBindVertexArray(mainQuadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    static float offset = 0.f;
    lightTransform.pos = glm::vec3(glm::sin(offset) * 300.f, 500.f, glm::cos(offset) * 300.f);
    offset += 0.03f;

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

    quadPass.Use();
    
    // render side quads
    for(int i = 0; i < texs.size(); i++)
    {
        texs[i]->Bind();
        glBindVertexArray(quadVAOs[i]);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    // wtf why is this not in reverse? maybe z testing?
    // main view
    texs[texIndex]->Bind();
    glBindVertexArray(mainQuadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // postfx

    glBindVertexArray(0);
}
