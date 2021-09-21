#pragma once

#include <unordered_set>
#include <unordered_map>
#include <string>
#include <functional>
#include <string.h>

#include <GL/glew.h>

#include "glm/glm.hpp"

#include "texture.h"

class Shader
{
public:
    // TODO: in the future shaders will be able to accept multiple materials
    unsigned int acceptedMaterialId;

    // TODO: maybe autoincrement on enums is nice?
    /*
    enum 
    {
        SCENE_PARAM_BINDING_POINT = 0,
        CAMERA_PARAM_BINDING_POINT,
        LIGHTING_BINDING_POINT,
        MATERIAL_PARAM_BINDING_POINT,
        MODEL_PARAM_BINDING_POINT
    };
    */
    static const unsigned int sceneParamBindingPoint    = 0;
    static const unsigned int cameraParamBindingPoint   = 1;
    static const unsigned int lightingBindingPoint      = 2;
    static const unsigned int materialParamBindingPoint = 3;
    static const unsigned int modelParamBindingPoint    = 4;

    void SetupUniformBlockBindings()
    {
        unsigned int sceneParamUniformBlockIndex = glGetUniformBlockIndex(shaderProgramId, "SceneParams");
        if (sceneParamUniformBlockIndex != GL_INVALID_INDEX)
            glUniformBlockBinding(shaderProgramId, sceneParamUniformBlockIndex, sceneParamBindingPoint);

        unsigned int cameraParamUniformBlockIndex = glGetUniformBlockIndex(shaderProgramId, "CameraParams");
        if (cameraParamUniformBlockIndex != GL_INVALID_INDEX)
            glUniformBlockBinding(shaderProgramId, cameraParamUniformBlockIndex, cameraParamBindingPoint);

        unsigned int lightingUniformBlockIndex = glGetUniformBlockIndex(shaderProgramId, "Lighting");
        if (lightingUniformBlockIndex != GL_INVALID_INDEX)
            glUniformBlockBinding(shaderProgramId, lightingUniformBlockIndex, lightingBindingPoint);

        unsigned int materialParamUniformBlockIndex = glGetUniformBlockIndex(shaderProgramId, "MaterialParams");
        if (materialParamUniformBlockIndex != GL_INVALID_INDEX)
            glUniformBlockBinding(shaderProgramId, materialParamUniformBlockIndex, materialParamBindingPoint);

        unsigned int modelParamUniformBlockIndex = glGetUniformBlockIndex(shaderProgramId, "ModelParams");
        if (modelParamUniformBlockIndex != GL_INVALID_INDEX)
            glUniformBlockBinding(shaderProgramId, modelParamUniformBlockIndex, modelParamBindingPoint);
    }

    unsigned int shaderProgramId;
    bool compilationSucceeded;
    char compilationErrorMsg[1024];

    // Yes mixing and matching is a pain in the ass, but so is calculating a hash of c style string
    std::unordered_set<std::string> boundUniforms;
    const char* name;

    Shader();
    Shader(const char *vsFilepath, const char *fsFilepath);
    Shader(const char *vsFilepath, const char *gsFilepath, const char *fsFilepath);
    ~Shader();

    void Use();

    GLint GetUniformLocation(const char* name);
    void SetUniform(const char *name, bool data);
    void SetUniform(const char *name, int data);
    void SetUniform(const char *name, float data);
    void SetUniform(const char *name, glm::vec2 data);
    void SetUniform(const char *name, glm::vec3 data);
    void SetUniform(const char *name, glm::vec4 data);
    void SetUniform(const char *name, glm::mat2 data);
    void SetUniform(const char *name, glm::mat3 data);
    void SetUniform(const char *name, glm::mat4 data);

    void AddDummyForUnboundTextures(int dummyTextureUnit);
    void ReportUnboundUniforms();

private:
    bool CheckCompileErrors(unsigned int shaderId, const char *shaderType);
};
