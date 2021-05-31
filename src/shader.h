#pragma once

#include <GL/glew.h>

#include "glm/glm.hpp"

class Shader
{
public:
    unsigned int shaderProgramId;
    bool compilationSucceeded;
    char compilationErrorMsg[1024];

    Shader();
    Shader(const char *vsFilepath, const char *fsFilepath);
    Shader(const char *vsFilepath, const char *gsFilepath, const char *fsFilepath);
    ~Shader();

    void Use();

    void SetUniform(const char *name, bool data);
    void SetUniform(const char *name, int data);
    void SetUniform(const char *name, float data);
    void SetUniform(const char *name, glm::vec2 data);
    void SetUniform(const char *name, glm::vec3 data);
    void SetUniform(const char *name, glm::vec4 data);
    void SetUniform(const char *name, glm::mat2 data);
    void SetUniform(const char *name, glm::mat3 data);
    void SetUniform(const char *name, glm::mat4 data);

private:
    bool CheckCompileErrors(unsigned int shaderId, const char *shaderType);
};
