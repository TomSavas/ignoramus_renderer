#pragma once

#include <unordered_set>
#include <unordered_map>
#include <string>
#include <functional>
#include <string.h>
#include <vector>

#include <GL/glew.h>

#include "glm/glm.hpp"

#include "texture.h"

#ifdef DEBUG
#define SHADER_PATH "../src/shaders/"
#else
#define SHADER_PATH "../src/shaders/"
//#define SHADER_PATH "./shaders/" TODO: update cmake to copy shaders next to bin
#endif

#define COMMON_SHADER SHADER_PATH "common.glsl"
#define VERT_COMMON_SHADER SHADER_PATH "common.vert"
#define FRAG_COMMON_SHADER SHADER_PATH "common.frag"
#define LIGHTING_COMMON_SHADER SHADER_PATH "lighting_common.frag"

struct ShaderDescriptor
{
    enum Type
    {
        COMPUTE_SHADER = 0,
        GEOMETRY_SHADER,
        VERTEX_SHADER,
        FRAGMENT_SHADER,

        SHADER_TYPE_COUNT
    };

    struct File
    {
        const char* filepath;
        Type type;
        const char* source;

        File(const char* filepath, Type type) : filepath(filepath), type(type), source(nullptr) {}
#define HARDCODED_SOURCE_FILEPATH "hardcoded"
        File(Type type, const char* source) : filepath(HARDCODED_SOURCE_FILEPATH), type(type), source(source) {}
    };

    struct Define
    {
        const char* define;
        const char* value;
    };

    std::vector<File> files;
    std::vector<Define> defines;
    ShaderDescriptor(std::vector<File> files = {}, std::vector<Define> defines = {}) : files(files), defines(defines) {}
    unsigned long Hash();
};

class Shader
{
friend class ShaderPool;

private:
    Shader() {}

public:
    // TODO: in the future shaders will be able to accept multiple materials
    unsigned int acceptedMaterialId;

    const char* name;
    ShaderDescriptor descriptor;
    unsigned int id;

#define INVALID_BINDING -1
    struct AutoBinding
    {
        const char* resource;
        int binding;
    };
    std::vector<AutoBinding> autoBindings;
    int GetBinding(const char* resource);

    static bool CompileShader(ShaderDescriptor& descriptor, Shader* shader);

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
        unsigned int sceneParamUniformBlockIndex = glGetUniformBlockIndex(id, "SceneParams");
        if (sceneParamUniformBlockIndex != GL_INVALID_INDEX)
            glUniformBlockBinding(id, sceneParamUniformBlockIndex, sceneParamBindingPoint);

        unsigned int cameraParamUniformBlockIndex = glGetUniformBlockIndex(id, "CameraParams");
        if (cameraParamUniformBlockIndex != GL_INVALID_INDEX)
            glUniformBlockBinding(id, cameraParamUniformBlockIndex, cameraParamBindingPoint);

        unsigned int lightingUniformBlockIndex = glGetUniformBlockIndex(id, "Lighting");
        if (lightingUniformBlockIndex != GL_INVALID_INDEX)
            glUniformBlockBinding(id, lightingUniformBlockIndex, lightingBindingPoint);

        unsigned int materialParamUniformBlockIndex = glGetUniformBlockIndex(id, "MaterialParams");
        if (materialParamUniformBlockIndex != GL_INVALID_INDEX)
            glUniformBlockBinding(id, materialParamUniformBlockIndex, materialParamBindingPoint);

        unsigned int modelParamUniformBlockIndex = glGetUniformBlockIndex(id, "ModelParams");
        if (modelParamUniformBlockIndex != GL_INVALID_INDEX)
            glUniformBlockBinding(id, modelParamUniformBlockIndex, modelParamBindingPoint);
    }

    // Yes mixing and matching is a pain in the ass, but so is calculating a hash of c style string
    std::unordered_set<std::string> boundUniforms;

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
};

struct Watchlist
{
    int inotifyFd;
    std::unordered_map<int, std::unordered_set<Shader*>> wdsToShaders;
    std::unordered_map<const char*, int> filenamesToWd;

    void Add(Shader& shader);
    void Remove(Shader& shader);
};

#define DEFAULT_SHADER ShaderPool::defaultShaderDescriptor
#define SCREEN_QUAD_TEXTURE_SHADER ShaderPool::screenQuadShaderDescriptor
struct ShaderPool
{
    ShaderPool();
    ~ShaderPool();

    static ShaderDescriptor defaultShaderDescriptor;
    static ShaderDescriptor screenQuadShaderDescriptor;

    Watchlist watchlist;
    std::unordered_map<unsigned long, Shader*> shaders;

    Shader& GetShader(ShaderDescriptor descriptor);

    void ReloadChangedShaders();
};
