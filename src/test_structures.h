#pragma once

#include <vector>

#include "render_pipeline.h"
#include "scene.h"
#include "shader.h"


Scene TestScene();

struct NamedPipeline
{
    const char* name;
    RenderPipeline pipeline;
};
std::vector<NamedPipeline> TestPipelines(ShaderPool& shaders);
