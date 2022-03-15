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
#include "hash.h"

unsigned long ShaderDescriptor::Hash()
{
    unsigned long hash = 5381;

    for (auto& file : files)
    {
        hash = Djb2((const unsigned char*)file.filepath, hash);
        hash = Djb2Byte((char)(file.type & 0x8), hash);
        //for (int i = 0; i < sizeof(file.type); i++)
        //{
        //    hash = Djb2Byte((char)(file.type << (i * 0x8) & 0x8), hash);
        //}
        //hash = Djb2((const unsigned char*)file.filepath, hash);
    }
    for (auto& define : defines)
    {
        hash = Djb2((const unsigned char*)define.define, hash);
        hash = Djb2((const unsigned char*)define.value, hash);
    }

    return hash;
}

ShaderDescriptor ShaderPool::defaultShaderDescriptor = ShaderDescriptor (
    { 
        ShaderDescriptor::File(SHADER_PATH "fallthrough.vert", ShaderDescriptor::Type::VERTEX_SHADER),
        ShaderDescriptor::File(SHADER_PATH "texture.frag", ShaderDescriptor::Type::FRAGMENT_SHADER),
    });
ShaderDescriptor ShaderPool::screenQuadShaderDescriptor = ShaderDescriptor (
    { 
        ShaderDescriptor::File(SHADER_PATH "fallthrough.vert", ShaderDescriptor::Type::VERTEX_SHADER),
        ShaderDescriptor::File(SHADER_PATH "texture.frag", ShaderDescriptor::Type::FRAGMENT_SHADER),
    });
ShaderPool::ShaderPool()
{
    watchlist.inotifyFd = inotify_init();
    fcntl(watchlist.inotifyFd, F_SETFL, fcntl(watchlist.inotifyFd, F_GETFL) | O_NONBLOCK);
}

ShaderPool::~ShaderPool()
{
    // TODO: delete all shaders/shaderPrograms
    // TODO: close all watches
    close(watchlist.inotifyFd);
}

Shader& ShaderPool::GetShader(ShaderDescriptor descriptor)
{
    auto existingShader = shaders.find(descriptor.Hash());
    if (existingShader != shaders.end())
    {
        return *existingShader->second;
    }

    Shader *shader = new Shader();
    bool compilationSucceeded = Shader::CompileShader(descriptor, shader);
    if (!compilationSucceeded)
    {
        assert(descriptor.Hash() != defaultShaderDescriptor.Hash());

        // We use the default shader, but set the descriptor to our desired one,
        // so that we can detect file changes and recompile the shader.
        *shader = GetShader(defaultShaderDescriptor);
        shader->descriptor = descriptor;
    }
    watchlist.Add(*shader);
    shaders[descriptor.Hash()] = shader;

    return *shader;
}

const char* ReadFile(const char *filepath, int emptyBytesAfterVersionDefine = 0)
{
    FILE *file = fopen(filepath, "rb");
    if (file == nullptr)
    {
        return nullptr;
    }

    fseek (file, 0, SEEK_END);
    unsigned int length = ftell(file) + 1;
    fseek (file, 0, SEEK_SET);
    char* fileContents = (char*) malloc(length + emptyBytesAfterVersionDefine);
    if (fileContents != nullptr)
    {
        fread(fileContents + emptyBytesAfterVersionDefine, 1, length, file);
        fileContents[length + emptyBytesAfterVersionDefine - 1] = 0;
    }
    memset(fileContents, ' ', emptyBytesAfterVersionDefine);

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

int Shader::GetBinding(const char* resource)
{
    for (auto autoBinding : autoBindings)
    {
        if (strcmp(resource, autoBinding.resource) == 0)
        {
            return autoBinding.binding;
       }
    }

    LOG_ERROR("Shader", "Could not find \"%s\" in 0x%X", resource, descriptor.Hash());
    return INVALID_BINDING;
}

/*static*/ bool Shader::CompileShader(ShaderDescriptor& descriptor, Shader* shader)
{
    shader->descriptor = descriptor;
    std::vector<unsigned int> shaderIds;

    LOG_INFO("Shader", "Compiling 0x%X...", descriptor.Hash());

    char* defines = (char*) malloc(sizeof(char) * 256 * descriptor.defines.size());
    defines[0] = '\0';
    for (auto& define : descriptor.defines)
    {
        char buf[256];
        sprintf(buf, "#define %s %s\n", define.define, define.value);
        strcat(defines, buf);
    }
    int defineLength = strlen(defines);
    if (defineLength > 0)
    {
        LOG_INFO("Shader", "\tDefines in 0x%X:\n%s", descriptor.Hash(), defines);
    }

    int bindingCount = 0;
    for (auto& file : descriptor.files)
    {
        LOG_INFO("Shader", "\tCompiling \"%s\" for 0x%X...", file.filepath, descriptor.Hash());

        // TODO: move to File, re-const descriptors
        bool fromFile = strcmp(file.filepath, HARDCODED_SOURCE_FILEPATH) != 0;
        if (fromFile)
        {
            // TODO: I think this is leaking somewhere...
            file.source = ReadFile(file.filepath, defineLength);

            // Auto bindings
            const char* bindingSuffixStr = "_AUTO_BINDING";
            char* bindingSuffix = strstr((char*)file.source, bindingSuffixStr);
            while (bindingSuffix != NULL)
            {
                char* bindingEnd = bindingSuffix + strlen(bindingSuffixStr) - 1;
                char* bindingStart = bindingEnd;
                while (*(bindingStart - 1) != ' ' && *(bindingStart - 1) != '=')
                {
                    bindingStart--;
                }
                
                char* resourceName = (char*) malloc(sizeof(char) * (bindingSuffix - bindingStart + 1));
                strncpy(resourceName, bindingStart, bindingSuffix - bindingStart);
                resourceName[bindingSuffix - bindingStart] = '\0';

                Shader::AutoBinding autoBinding { resourceName, shader->autoBindings.size() };
                shader->autoBindings.push_back(autoBinding);
                LOG_INFO("Shader", "\t\tAuto binding \"%s\" to %d in 0x%X", autoBinding.resource, autoBinding.binding, descriptor.Hash());

                memset(bindingStart, ' ', bindingEnd - bindingStart + 1);

                char num[8];
                sprintf(num, "%d", bindingCount++);
                for (int i = 0; i < strlen(num); i++)
                {
                    *(bindingStart + 1 + i) = num[i]; 
                }

                bindingSuffix = strstr(bindingEnd + strlen(bindingSuffixStr), bindingSuffixStr);
            }
             
            // Defines
            if (defineLength > 0)
            {
                // Move version to the top
                // Okay to cast away the const, not a hardcoded string
                char* versionStartPtr = strstr((char*) file.source, "#version");
                char* versionEndPtr = strstr((char*) file.source, "\n");
                int versionLength = versionEndPtr - versionStartPtr + 1;
                memmove((char*) file.source, versionStartPtr, versionLength);

                // Remove old version
                memset(versionStartPtr, ' ', versionLength);

                // Insert defines
                memmove((void*) file.source + versionLength, defines, defineLength);
            }
        }

        unsigned int id = glCreateShader(ToGlType(file.type));
        glShaderSource(id, 1, &file.source, NULL);
        glCompileShader(id);

        int compilationSucceeded = 0;
        glGetShaderiv(id, GL_COMPILE_STATUS, &compilationSucceeded);
        if (compilationSucceeded == 0)
        {
            char errorMsg[1024];
            glGetShaderInfoLog(id, 1024, NULL, (GLchar*) &errorMsg);
            LOG_ERROR("Shader", "\t\tFailed compiling \"%s\" for 0x%X:\n\t%s", file.filepath, descriptor.Hash(), errorMsg);

            char* annotatedSource = annotateLineNumbers(file.source);
            LOG_ERROR("Shader", "\t\tSource:\n%s", annotatedSource);
            free(annotatedSource);
        }

        shaderIds.push_back(id);
    }
    free(defines);

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
        LOG_ERROR("Shader", "\tFailed linking 0x%X:\n\t%s", descriptor.Hash(), errorMsg);
        glDeleteProgram(id);

        return false;
    }

    LOG_INFO("Shader", "\tSuccessfully compiled 0x%X", descriptor.Hash());
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
        Shader replacementShader;
        bool compilationSucceeded = Shader::CompileShader(shader->descriptor, &replacementShader);
        if (compilationSucceeded)
        {
            assert(shader->descriptor.Hash() != defaultShaderDescriptor.Hash());
            *shader = replacementShader;
        }
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
    char uniformName[128];

    int count = 0;
    glGetProgramiv(id, GL_ACTIVE_UNIFORMS, &count);
    for (int i = 0; i < count; i++)
    {
        glGetActiveUniform(id, i, sizeof(uniformName), &unused, &unused, &type, uniformName);

        // Definitely not exhaustive...
        bool isTexture = type == GL_SAMPLER_1D || type == GL_SAMPLER_2D || type == GL_SAMPLER_3D;
        bool isCubemap = type == GL_SAMPLER_CUBE;
        if (boundUniforms.find(std::string(uniformName)) == boundUniforms.end())
        {
            if (isTexture)
            {
                //LOG_WARN("Shader", "\"%s\". Unbound tex found: %s. Binding dummy texture.", name, uniformName);
                // Doesn't need activating, we're always keeping our dummy texture unit active
                SetUniform(uniformName, dummyTextureUnit);
            }
            else if (isCubemap)
            {
                // TODO
                //LOG_WARN("Shader", "\"%s\". Unbound cubemap found: %s. Binding dummy cubemap.", name, uniformName);
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
            LOG_ERROR("Unbound uniform: %s. Type: 0x%X", name, type);
        }
    }
}
