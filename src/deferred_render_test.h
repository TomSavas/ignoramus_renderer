#pragma once

#include <vector>

#include "GLFW/glfw3.h"

#include "camera.h"
#include "model.h"
#include "shader.h"

struct DeferredTest
{
    unsigned int gBuffer;

    unsigned int finalFb;
    unsigned int finalTexId;

    unsigned int depthStencil;
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

    Transform lightTransform;
    Transform groundTransform;

    Camera camera;

    DeferredTest();
    void Render();
};
