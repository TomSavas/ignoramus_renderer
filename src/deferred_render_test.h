#pragma once

#include <vector>

#include "GLFW/glfw3.h"

#include "camera.h"
#include "model.h"
#include "shader.h"

struct DeferredTest
{
    // control
    bool pointShadows = false;
    bool directionalShadows = true;
    bool ssao = true;
    //bool pointLight;

    unsigned int gBuffer;

    unsigned int pointShadowFBO;
    unsigned int directionalShadowFBO;
    unsigned int finalFb;
    unsigned int finalTexId;

    unsigned int shadowDepthCubemap;
    unsigned int shadowDepth;

    float directionalBias;
    float directionalAngleBias;
    float pointBias;
    float pointAngleBias;

    float gamma = 2.2;

    unsigned int depthStencil;
    Shader pointShadowPass;
    Shader directionalShadowPass;
    Shader geometryPass;
    Shader lightingPass;
    Shader quadPass;

    Model *groundModel;
    Model *lightModel;
    Model *model;
    unsigned int gPosition, gNormal, gAlbedo, gSpec;

    Texture *modelDiffuseTex;
    Texture *modelNormalTex;
    Texture *modelSpecularTex;

    Texture *posTex;
    Texture *normalTex;
    Texture *albedoTex;
    Texture *specTex;
    Texture *finalTex;

    GLFWwindow* window;

    unsigned int mainQuadVAO;
    std::vector<unsigned int> quadVAOs;

    Transform pointLightTransform;
    Transform directionalLightTransform;
    Transform groundTransform;

    Camera camera;

    DeferredTest();
    void Render();
};
