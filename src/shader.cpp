#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glm/gtc/type_ptr.hpp>

#include "log.h"

#include "shader.h"

bool ReadFile(const char *filepath, char **fileContents, unsigned int &length)
{
    FILE *file = fopen(filepath, "rb");
    
    if (!file)
        return false;

    fseek (file, 0, SEEK_END);
    length = ftell(file) + 1;
    fseek (file, 0, SEEK_SET);

    *fileContents = (char*) malloc(length);
    if (!*fileContents)
        return false;

    fread(*fileContents, 1, length, file);
    (*fileContents)[length-1] = 0;
    fclose(file);

    return true;
}

Shader::Shader()
{
    compilationSucceeded = false;
    strcpy(compilationErrorMsg, "Shader not initialized");
}

Shader::Shader(const char *vsFilepath, const char *fsFilepath)
{
    unsigned int vsLength;
    char *vsCode;
    unsigned int fsLength;
    char *fsCode;
    if (!ReadFile(vsFilepath, &vsCode, vsLength) || !ReadFile(fsFilepath, &fsCode, fsLength))
    {
        compilationSucceeded = false;
        strcpy(compilationErrorMsg, "Failed reading shaders from files");

        return;
    }

    unsigned int vertexShaderId = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShaderId, 1, &vsCode, NULL);
    glCompileShader(vertexShaderId);
    if (CheckCompileErrors(vertexShaderId, "VERTEX"))
    {
        free(vsCode);
        free(fsCode);

        glDeleteShader(vertexShaderId);
        return;
    }

    unsigned int fragmentShaderId = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShaderId, 1, &fsCode, NULL);
    glCompileShader(fragmentShaderId);
    if (CheckCompileErrors(fragmentShaderId, "FRAGMENT"))
    {
        free(vsCode);
        free(fsCode);

        glDeleteShader(vertexShaderId);
        glDeleteShader(fragmentShaderId);
        return;
    }

    shaderProgramId = glCreateProgram();
    glAttachShader(shaderProgramId, vertexShaderId);
    glAttachShader(shaderProgramId, fragmentShaderId);
    glLinkProgram(shaderProgramId);

    CheckCompileErrors(shaderProgramId, "PROGRAM");
    free(vsCode);
    free(fsCode);
    glDeleteShader(vertexShaderId);
    glDeleteShader(fragmentShaderId);

    SetupUniformBlockBindings();

    if (!compilationSucceeded)
        printf("FAILED CREATING SHADER PROGRAM (%s, %s) :\n%s\n", vsFilepath, fsFilepath, compilationErrorMsg);
    else
        printf("Shader compilation succeeded (%s, %s)\n", vsFilepath, fsFilepath);
}

Shader::Shader(const char *vsFilepath, const char *gsFilepath, const char *fsFilepath)
{
    unsigned int vsLength;
    char *vsCode;
    unsigned int gsLength;
    char *gsCode;
    unsigned int fsLength;
    char *fsCode;
    if (!ReadFile(vsFilepath, &vsCode, vsLength) || !ReadFile(gsFilepath, &gsCode, gsLength) || !ReadFile(fsFilepath, &fsCode, fsLength))
    {
        compilationSucceeded = false;
        strcpy(compilationErrorMsg, "Failed reading shaders from files");

        return;
    }

    unsigned int vertexShaderId = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShaderId, 1, &vsCode, NULL);
    glCompileShader(vertexShaderId);
    if (CheckCompileErrors(vertexShaderId, "VERTEX"))
    {
        free(vsCode);
        free(gsCode);
        free(fsCode);

        glDeleteShader(vertexShaderId);
        return;
    }

    unsigned int geometryShaderId = glCreateShader(GL_GEOMETRY_SHADER);
    glShaderSource(geometryShaderId, 1, &gsCode, NULL);
    glCompileShader(geometryShaderId);
    if (CheckCompileErrors(geometryShaderId, "GEOMETRY"))
    {
        free(vsCode);
        free(gsCode);
        free(fsCode);

        glDeleteShader(vertexShaderId);
        glDeleteShader(geometryShaderId);
        return;
    }

    unsigned int fragmentShaderId = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShaderId, 1, &fsCode, NULL);
    glCompileShader(fragmentShaderId);
    if (CheckCompileErrors(fragmentShaderId, "FRAGMENT"))
    {
        free(vsCode);
        free(gsCode);
        free(fsCode);

        glDeleteShader(vertexShaderId);
        glDeleteShader(geometryShaderId);
        glDeleteShader(fragmentShaderId);
        return;
    }

    shaderProgramId = glCreateProgram();
    glAttachShader(shaderProgramId, vertexShaderId);
    glAttachShader(shaderProgramId, geometryShaderId);
    glAttachShader(shaderProgramId, fragmentShaderId);
    glLinkProgram(shaderProgramId);

    CheckCompileErrors(shaderProgramId, "PROGRAM");
    free(vsCode);
    free(gsCode);
    free(fsCode);
    glDeleteShader(vertexShaderId);
    glDeleteShader(geometryShaderId);
    glDeleteShader(fragmentShaderId);

    SetupUniformBlockBindings();

    if (!compilationSucceeded)
        printf("FAILED CREATING SHADER PROGRAM (%s, %s, %s) :\n%s\n", vsFilepath, gsFilepath, fsFilepath, compilationErrorMsg);
    else
        printf("Shader compilation succeeded (%s, %s, %s)\n", vsFilepath, gsFilepath, fsFilepath);
}

Shader::~Shader()
{

}

void Shader::Use()
{
    glUseProgram(shaderProgramId);
    boundUniforms.clear();
}

bool Shader::CheckCompileErrors(unsigned int shaderId, const char *shaderType)
{
    if (strcmp(shaderType, "VERTEX") == 0 || strcmp(shaderType, "FRAGMENT") == 0 || strcmp(shaderType, "GEOMETRY") == 0)
    {
        glGetShaderiv(shaderId, GL_COMPILE_STATUS, (GLint*) &compilationSucceeded);
        if(!compilationSucceeded)
            glGetShaderInfoLog(shaderId, 1024, NULL, (GLchar*) &compilationErrorMsg);
    }
    else if (strcmp(shaderType, "PROGRAM") == 0)
    {
        glGetProgramiv(shaderId, GL_LINK_STATUS, (GLint*) &compilationSucceeded);
        if(!compilationSucceeded)
            glGetProgramInfoLog(shaderId, 1024, NULL, (GLchar*) &compilationErrorMsg);
    }
    else
    {
        compilationSucceeded = false;
    }

    if (!compilationSucceeded)
    {
        strcat(&compilationErrorMsg[0], "[");
        strcat(&compilationErrorMsg[0], shaderType);
        strcat(&compilationErrorMsg[0], "]\n");
    }

    return !compilationSucceeded;
}

GLint Shader::GetUniformLocation(const char* name)
{
    GLint location = glGetUniformLocation(shaderProgramId, name);
    assert(location != GL_INVALID_VALUE);
    boundUniforms.insert(std::string(name));

    return location;
}

void Shader::SetUniform(const char *name, bool data)
{
    glUniform1i(GetUniformLocation(name), (int)data); 
}

void Shader::SetUniform(const char *name, int data)
{
    glUniform1i(GetUniformLocation(name), data); 
}

void Shader::SetUniform(const char *name, float data)
{
    glUniform1f(GetUniformLocation(name), data); 
}

void Shader::SetUniform(const char *name, glm::vec2 data)
{
    glUniform2fv(GetUniformLocation(name), 1, &data[0]); 
}

void Shader::SetUniform(const char *name, glm::vec3 data)
{
    glUniform3fv(GetUniformLocation(name), 1, &data[0]); 
}

void Shader::SetUniform(const char *name, glm::vec4 data)
{
    glUniform4fv(GetUniformLocation(name), 1, &data[0]); 
}

void Shader::SetUniform(const char *name, glm::mat2 data)
{
    glUniformMatrix2fv(GetUniformLocation(name), 1, GL_FALSE, &data[0][0]); 
}

void Shader::SetUniform(const char *name, glm::mat3 data)
{
    glUniformMatrix3fv(GetUniformLocation(name), 1, GL_FALSE, &data[0][0]); 
}

void Shader::SetUniform(const char *name, glm::mat4 data)
{
    //glUniformMatrix4fv(glGetUniformLocation(shaderProgramId, name), 1, GL_FALSE, &data[0][0]); 
    glUniformMatrix4fv(GetUniformLocation(name), 1, GL_FALSE, glm::value_ptr(data)); 
}

void Shader::AddDummyForUnboundTextures(int dummyTextureUnit)
{
    int unused;
    GLenum type;
    char name[128];

    int count = 0;
    glGetProgramiv(shaderProgramId, GL_ACTIVE_UNIFORMS, &count);
    for (int i = 0; i < count; i++)
    {
        glGetActiveUniform(shaderProgramId, i, sizeof(name), &unused, &unused, &type, name);

        // Definitely not exhaustive...
        bool isTexture = type == GL_SAMPLER_1D || type == GL_SAMPLER_2D || type == GL_SAMPLER_3D;
        bool isCubemap = type == GL_SAMPLER_CUBE;
        if (boundUniforms.find(std::string(name)) == boundUniforms.end())
        {
            if (isTexture)
            {
                LOG_WARN("Unbound tex found: %s. Binding dummy texture.", name);
                // Doesn't need activating, we're always keeping our dummy texture unit active
                SetUniform(name, dummyTextureUnit);
            }
            else if (isCubemap)
            {
                // TODO
                LOG_WARN("Unbound cubemap found: %s. Binding dummy cubemap.", name);
                assert(false);
            }
        }
    }
}

void Shader::ReportUnboundUniforms()
{
    // Reports on free floating uniforms. Can't deal with uniform buffers though

    int unused;
    int count = 0;
    GLenum type;
    char name[128];
    glGetProgramiv(shaderProgramId, GL_ACTIVE_UNIFORMS, &count);
    for (int i = 0; i < count; i++)
    {
        glGetActiveUniform(shaderProgramId, (GLuint)i, 128, &unused, &unused, &type, name);

        if (boundUniforms.find(std::string(name)) == boundUniforms.end())
        {
            LOG_ERROR("Unbound uniform: %s. Type: %x", name, type);
        }
    }
}
