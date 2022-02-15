#include <glm/gtc/type_ptr.hpp>

#include "scene.h"
#include "shader.h"


Scene::Scene() : globalAttachments("Global attachment container", PassSettings::DefaultRenderpassSettings())
{
    glGenBuffers(1, &sceneParamsUboId);
    glBindBufferBase(GL_UNIFORM_BUFFER, Shader::sceneParamBindingPoint, sceneParamsUboId);

    glGenBuffers(1, &mainCameraParamsUboId);
    glBindBufferBase(GL_UNIFORM_BUFFER, Shader::cameraParamBindingPoint, mainCameraParamsUboId);

    glGenBuffers(1, &lightingUboId);
    glBindBufferBase(GL_UNIFORM_BUFFER, Shader::lightingBindingPoint, lightingUboId);
}

void Scene::BindSceneParams()
{
    // TODO: don't update everytime
    glBindBuffer(GL_UNIFORM_BUFFER, sceneParamsUboId);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(sceneParams), &sceneParams, GL_STATIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void Scene::BindCameraParams()
{
    // TODO: don't update everytime
    glBindBuffer(GL_UNIFORM_BUFFER, mainCameraParamsUboId);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(mainCameraParams), &mainCameraParams, GL_STATIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void Scene::BindLighting()
{
    // TODO: don't update everytime
    glBindBuffer(GL_UNIFORM_BUFFER, lightingUboId);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(lighting), &lighting, GL_STATIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}
