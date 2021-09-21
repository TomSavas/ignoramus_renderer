#pragma once

#include <vector>

#include "GLFW/glfw3.h"

#include "camera.h"
#include "model.h"
#include "shader.h"
#include "scene.h"
#include "render_pipeline.h"

struct DeferredTest
{
    Camera camera;

    Scene scene;
    RenderPipeline pipeline;

    // Really bad place to keep this. Will do for now though
    unsigned int ubo;

    DeferredTest();
    void Render();
};
