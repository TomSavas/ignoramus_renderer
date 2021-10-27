#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glm/gtc/type_ptr.hpp>
#include <sys/inotify.h>
#include <unistd.h>
#include <unordered_set>
#include <fcntl.h>
#include <errno.h>

#include "log.h"

#include "shader.h"

ShaderPool::ShaderPool()
{
    AddShader(DEFAULT_SHADER, 
        ShaderDescriptor(
            { 
                //ShaderDescriptor::File(ShaderDescriptor::Type::VERTEX_SHADER, ""),
                //ShaderDescriptor::File(ShaderDescriptor::Type::FRAGMENT_SHADER, ""),
                ShaderDescriptor::File(SHADER_PATH "texture.vert", ShaderDescriptor::Type::VERTEX_SHADER),
                ShaderDescriptor::File(SHADER_PATH "texture.frag", ShaderDescriptor::Type::FRAGMENT_SHADER),
            }));
    AddShader(SCREEN_QUAD_TEXTURE_SHADER, 
        ShaderDescriptor(
            { 
                ShaderDescriptor::File(SHADER_PATH "texture.vert", ShaderDescriptor::Type::VERTEX_SHADER),
                ShaderDescriptor::File(SHADER_PATH "texture.frag", ShaderDescriptor::Type::FRAGMENT_SHADER),
            }));

    watchlist.inotifyFd = inotify_init();
    fcntl(watchlist.inotifyFd, F_SETFL, fcntl(watchlist.inotifyFd, F_GETFL) | O_NONBLOCK);
}

ShaderPool::~ShaderPool()
{
    // TODO: delete all shaders/shaderPrograms
    // TODO: close all watches
    close(watchlist.inotifyFd);
}

Shader& ShaderPool::GetShader(const char* name)
{
    auto shader = shaders.find(name);
    if (shader == shaders.end())
    {
        return GetShader(DEFAULT_SHADER);
    }

    return *shader->second;
}

const char* ReadFile(const char *filepath)
{
    FILE *file = fopen(filepath, "rb");
    if (file == nullptr)
    {
        return nullptr;
    }

    fseek (file, 0, SEEK_END);
    unsigned int length = ftell(file) + 1;
    fseek (file, 0, SEEK_SET);
    char* fileContents = (char*) malloc(length);
    if (fileContents != nullptr)
    {
        fread(fileContents, 1, length, file);
        fileContents[length-1] = 0;
    }

    fclose(file);

    return fileContents;
}

GLenum ToGlType(ShaderDescriptor::Type type)
{
    switch (type)
    {
        case ShaderDescriptor::COMPUTE_SHADER:
            return GL_COMPUTE_SHADER;
        case ShaderDescriptor::GEOMETRY_SHADER:
            return GL_GEOMETRY_SHADER;
        case ShaderDescriptor::VERTEX_SHADER:
            return GL_VERTEX_SHADER;
        case ShaderDescriptor::FRAGMENT_SHADER:
            return GL_FRAGMENT_SHADER;
        default:
            LOG_ERROR(__func__, "Cannot convert shader tpye to GL type. Invalid type: %d", type);
            return GL_FRAGMENT_SHADER;
    }
}

bool CompileShader(const char* name, ShaderDescriptor& descriptor, Shader* shader)
{
    shader->descriptor = descriptor;
    std::vector<unsigned int> shaderIds;
    for (auto& file : descriptor.files)
    {
        // TODO: move to File, re-const descriptors
        bool fromFile = strcmp(file.filepath, HARDCODED_SOURCE_FILEPATH) != 0;
        if (fromFile)
        {
            // Safe, if fromFile this cannot be hardcoded -- this shader is being reloaded
            if (file.source != nullptr)
            {
                free((char*) file.source);
            }
            // TODO: I think this is leaking somewhere...
            file.source = ReadFile(file.filepath);
        }

        LOG_INFO("Shader", "Compiling \"%s\" for \"%s\"...", file.filepath, name);
        unsigned int id = glCreateShader(ToGlType(file.type));
        glShaderSource(id, 1, &file.source, NULL);
        glCompileShader(id);

        int compilationSucceeded = 0;
        glGetShaderiv(id, GL_COMPILE_STATUS, &compilationSucceeded);
        if (compilationSucceeded == 0)
        {
            char errorMsg[1024];
            glGetShaderInfoLog(id, 1024, NULL, (GLchar*) &errorMsg);
            LOG_ERROR("Shader", "Failed compiling \"%s\" for \"%s\":\n\t%s", file.filepath, name, errorMsg);
        }

        shaderIds.push_back(id);
    }

    unsigned int id = glCreateProgram();
    for (unsigned int shaderId : shaderIds)
    {
        glAttachShader(id, shaderId);
    }
    glLinkProgram(id);
    shader->id = id;

    for (unsigned int shaderId : shaderIds)
    {
        glDeleteShader(shaderId);
    }

    int compilationSucceeded = 0;
    glGetProgramiv(id, GL_LINK_STATUS, &compilationSucceeded);
    if (compilationSucceeded == 0)
    {
        char errorMsg[1024];
        glGetProgramInfoLog(id, 1024, NULL, (GLchar*) &errorMsg);
        LOG_ERROR("Shader", "Failed linking \"%s\":\n\t", name, errorMsg);
        glDeleteProgram(id);

        return false;
    }

    LOG_INFO("Shader", "Successfully compiled \"%s\"", name);
    shader->SetupUniformBlockBindings();
    return true;
}

void Watchlist::Add(Shader& shader)
{
    for (auto file : shader.descriptor.files)
    {
        if (access(file.filepath, F_OK) != 0)
        {
            // Probably hardcoded shader source. Nothing to watch...
            continue;
        }

        int wd;
        auto existingWd = filenamesToWd.find(file.filepath);
        if (existingWd != filenamesToWd.end())
        {
            wd = existingWd->second;
        }
        else
        {
            wd = inotify_add_watch(inotifyFd, file.filepath, IN_MODIFY);
        }

        LOG_INFO("Watchlist", "Adding \"%s\" to watchlist", file.filepath);
        wdsToShaders[wd].insert(&shader);
    }
}

void Watchlist::Remove(Shader& shader)
{
    for (auto file : shader.descriptor.files)
    {
        int wd = filenamesToWd[file.filepath];
        wdsToShaders[wd].erase(&shader);

        if (wdsToShaders[wd].size() == 0)
        {
            LOG_INFO("Watchlist", "Watchlist for \"%s\" is empty. Clearing", file.filepath);

            wdsToShaders.erase(wd);
            filenamesToWd.erase(file.filepath);

            inotify_rm_watch(inotifyFd, wd);
        }
    }
}

Shader& ShaderPool::AddShader(const char* name, ShaderDescriptor descriptor)
{
    Shader* shader = new Shader();
    shader->name = name;
    bool compiledSuccessfully = CompileShader(name, descriptor, shader);
    if (!compiledSuccessfully)
    {
        *shader = GetShader(DEFAULT_SHADER);
    }

    auto existingShader = shaders.find(name);
    if (existingShader == shaders.end())
    {
        shaders[name] = shader;
        if (!compiledSuccessfully)
        {
            LOG_ERROR("Shader", "Failed compiling \"%s\". Using default shader", name);
        }
        else
        {
            watchlist.Add(*shader);
        }

        return *shader;
    }
    else
    {
        if (compiledSuccessfully)
        {
            *existingShader->second = *shader;
            LOG_INFO("Shader", "Replacing existing \"%s\" shader with a new one", name);
        }
        else
        {
            delete shader;
            LOG_ERROR("Shader", "Failed compiling new version of \"%s\". Preserving the old version", name);
        }

        return *existingShader->second;
    }
}

void ShaderPool::ReloadChangedShaders()
{
    inotify_event events[32];
    int bytesRead = read(watchlist.inotifyFd, events, sizeof(events));
    int eventsRead = bytesRead / sizeof(inotify_event);

    if (bytesRead == -1)
    {
        // Expected behaviour when O_NONBLOCK is set
        if (errno != EAGAIN)
        {
            LOG_WARN("ShaderPool", "Failed reading inotify events: %d", errno);
            return;
        }
    }

    std::unordered_set<Shader*> shadersToUpdate;
    for (int i = 0; i < eventsRead; i++)
    {
        inotify_event event = events[i];
        shadersToUpdate.insert(watchlist.wdsToShaders[event.wd].begin(), watchlist.wdsToShaders[event.wd].end());
    }

    for (auto* shader : shadersToUpdate)
    {
        AddShader(shader->name, shader->descriptor);
    }
}

void Shader::Use()
{
    glUseProgram(id);
    boundUniforms.clear();
}

GLint Shader::GetUniformLocation(const char* name)
{
    GLint location = glGetUniformLocation(id, name);
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
    //glUniformMatrix4fv(glGetUniformLocation(id, name), 1, GL_FALSE, &data[0][0]); 
    glUniformMatrix4fv(GetUniformLocation(name), 1, GL_FALSE, glm::value_ptr(data)); 
}

void Shader::AddDummyForUnboundTextures(int dummyTextureUnit)
{
    int unused;
    GLenum type;
    char name[128];

    int count = 0;
    glGetProgramiv(id, GL_ACTIVE_UNIFORMS, &count);
    for (int i = 0; i < count; i++)
    {
        glGetActiveUniform(id, i, sizeof(name), &unused, &unused, &type, name);

        // Definitely not exhaustive...
        bool isTexture = type == GL_SAMPLER_1D || type == GL_SAMPLER_2D || type == GL_SAMPLER_3D;
        bool isCubemap = type == GL_SAMPLER_CUBE;
        if (boundUniforms.find(std::string(name)) == boundUniforms.end())
        {
            if (isTexture)
            {
                //LOG_WARN("Unbound tex found: %s. Binding dummy texture.", name);
                // Doesn't need activating, we're always keeping our dummy texture unit active
                SetUniform(name, dummyTextureUnit);
            }
            else if (isCubemap)
            {
                // TODO
                //LOG_WARN("Unbound cubemap found: %s. Binding dummy cubemap.", name);
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
    glGetProgramiv(id, GL_ACTIVE_UNIFORMS, &count);
    for (int i = 0; i < count; i++)
    {
        glGetActiveUniform(id, (GLuint)i, 128, &unused, &unused, &type, name);

        if (boundUniforms.find(std::string(name)) == boundUniforms.end())
        {
            LOG_ERROR("Unbound uniform: %s. Type: %x", name, type);
        }
    }
}
