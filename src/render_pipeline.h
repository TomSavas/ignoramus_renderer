#pragma once 

#include <vector>
#include <unordered_map>

#include "glm/glm.hpp"

#include "mesh.h"
#include "perf_data.h"
#include "scene.h"

enum class AttachmentFormat
{
    UINT_1,
    FLOAT_1,
    FLOAT_2,
    FLOAT_3,
    FLOAT_4,
    DEPTH,
    DEPTH_STENCIL,

    SSBO,
    ATOMIC_COUNTER,
};
GLenum ToGLInternalFormat(AttachmentFormat format);
GLenum ToGLFormat(AttachmentFormat format);
GLenum ToGLType(AttachmentFormat format);

struct AttachmentClearOpts
{
    glm::vec4 color;
    AttachmentClearOpts(glm::vec4 color = glm::vec4(1.f, 0.f, 1.f, 0.f)) : color(color) {}
};

struct RenderpassAttachment
{
    const char* name;
    AttachmentFormat format;

#define INVALID_ATTACHMENT_INDEX -1
    GLenum attachmentIndex;

    bool hasSeparateClearOpts;
    AttachmentClearOpts clearOpts;
    long size;

    RenderpassAttachment() {}
    RenderpassAttachment(const char* name, AttachmentFormat format) : name(name), format(format), hasSeparateClearOpts(false), attachmentIndex(INVALID_ATTACHMENT_INDEX) {}
    RenderpassAttachment(const char* name, AttachmentFormat format, AttachmentClearOpts clearOpts) : name(name), format(format), clearOpts(clearOpts), hasSeparateClearOpts(true), attachmentIndex(INVALID_ATTACHMENT_INDEX) {}

    // TODO: remake so that each enum param has a separate "constructor"
    static RenderpassAttachment SSBO(const char* name, long size) 
    {
        RenderpassAttachment attachment(name, AttachmentFormat::SSBO);
        attachment.size = size;

        return attachment;
    }

    static RenderpassAttachment AtomicCounter(const char* name) 
    {
        RenderpassAttachment attachment(name, AttachmentFormat::ATOMIC_COUNTER);
        attachment.size = sizeof(GLuint);

        return attachment;
    }

    unsigned int id;
};

struct PassSettings
{
    bool ignoreApplication;
    bool ignoreClear;

    std::vector<GLenum> enable;   
    GLenum cullFace;

    GLboolean depthMask;
    GLenum depthFunc;

    GLenum colorMask[4];
    glm::vec4 clearColor;
    GLenum clear;

    GLenum blendEquation;
    GLenum srcBlendFactor;
    GLenum dstBlendFactor;

    void Clear();
    void Apply();
    void Apply(PassSettings& previousSettings);

    static PassSettings DefaultSettings();
    static PassSettings DefaultRenderpassSettings();
    static PassSettings DefaultSubpassSettings();
    static PassSettings DefaultOutputRenderpassSettings();
};

struct SubpassAttachment
{
    enum AttachType
    {
        AS_COLOR,
        AS_DEPTH,
        AS_TEXTURE,
        AS_BLIT,

        AS_IMAGE,
        AS_SSBO,
        AS_ATOMIC_COUNTER
    };

    RenderpassAttachment* renderpassAttachment;
    AttachType type;

    bool hasSeparateClearOpts;
    AttachmentClearOpts clearOpts;

    SubpassAttachment(RenderpassAttachment* renderpassAttachment, AttachType type, const char* useFor = "\0") : renderpassAttachment(renderpassAttachment), type(type), hasSeparateClearOpts(false), useFor(useFor) {}
    SubpassAttachment(RenderpassAttachment* renderpassAttachment, AttachType type, AttachmentClearOpts clearOpts, const char* useFor = "\0") : renderpassAttachment(renderpassAttachment), type(type), clearOpts(clearOpts), hasSeparateClearOpts(true), useFor(useFor) {}

    // Only if AS_TEXTURE or AS_BLIT
    const char* useFor;
};

struct Subpass
{
    const char* name;
    Shader* shader;
    MeshTag acceptedMeshTags;
    std::vector<SubpassAttachment> attachments;
    PassSettings settings;
    std::vector<GLenum> colorAttachmentsToActivate;

    PerfData perfData;
};

struct Renderpass
{
    const char* name;
    PassSettings settings;

    unsigned int fbo;
    std::vector<Subpass*> subpasses;
    std::vector<GLenum> allColorAttachmentIndices;
    std::vector<RenderpassAttachment*> attachments;
    RenderpassAttachment* outputAttachment = nullptr;

    PerfData perfData;

    Renderpass(const char* name, PassSettings settings) : name(name), settings(settings), outputAttachment(nullptr), fbo(GL_INVALID_VALUE) {}

    Subpass& AddSubpass(const char* name, Shader* shader, MeshTag acceptedMeshTags, std::vector<SubpassAttachment> attachments, PassSettings passSettings = PassSettings::DefaultSubpassSettings());
    Subpass& InsertSubpass(int index, const char* name, Shader* shader, MeshTag acceptedMeshTags, std::vector<SubpassAttachment> attachments, PassSettings passSettings = PassSettings::DefaultSubpassSettings());
    Subpass& AddSubpass(const char* name, Shader* shader, MeshTag acceptedMeshTags, std::vector<RenderpassAttachment*> attachments, SubpassAttachment::AttachType typeForAllAttachments, PassSettings passSettings = PassSettings::DefaultSubpassSettings());

    RenderpassAttachment& AddAttachment(RenderpassAttachment attachment);
    RenderpassAttachment& GetAttachment(const char* name);

    RenderpassAttachment& AddOutputAttachment();
};

struct RenderPipeline
{
    std::vector<Renderpass*> passes;
    PerfData perfData;

    RenderPipeline();

    Renderpass& AddPass(const char* name, PassSettings passSettings = PassSettings::DefaultRenderpassSettings());
    Renderpass& AddOutputPass(ShaderPool& shaders);

    // Configurues all attachment in the order they are attached to the pipeline
    bool ConfigureAttachments();

    void Render(Scene& scene, ShaderPool& shaders);

    // TODO: a horrible place to put this
    int dummyTextureUnit;
    static RenderpassAttachment& DummyAttachment();

    GLuint timeQuery;
    unsigned int materialUbo;
};
