#include <vector>
#include <unordered_map>

#include "mesh.h"
#include "perf_data.h"

enum class AttachmentFormat
{
    FLOAT_1,
    FLOAT_3,
    FLOAT_4,
    DEPTH,
    DEPTH_STENCIL
};
GLenum ToGLInternalFormat(AttachmentFormat format);
GLenum ToGLFormat(AttachmentFormat format);
GLenum ToGLType(AttachmentFormat format);

struct RenderpassAttachment
{
    const char* name;
    AttachmentFormat format;

    RenderpassAttachment(const char* name, AttachmentFormat format) : name(name), format(format) {}

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
        AS_BLIT
    };

    RenderpassAttachment* renderpassAttachment;
    AttachType type;

    SubpassAttachment(RenderpassAttachment* renderpassAttachment, AttachType type, const char* useFor = "\0") : renderpassAttachment(renderpassAttachment), type(type), useFor(useFor) {}

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
    std::vector<GLenum> colorAttachmentIndices;
    std::vector<RenderpassAttachment*> attachments;
    RenderpassAttachment* outputAttachment = nullptr;

    PerfData perfData;

    Renderpass(const char* name, PassSettings settings) : name(name), settings(settings), outputAttachment(nullptr), fbo(GL_INVALID_VALUE) {}

    Subpass& AddSubpass(const char* name, Shader* shader, MeshTag acceptedMeshTags, std::vector<SubpassAttachment> attachments, PassSettings passSettings = PassSettings::DefaultSubpassSettings());
    Subpass& AddSubpass(const char* name, Shader* shader, MeshTag acceptedMeshTags, std::vector<RenderpassAttachment*> attachments, SubpassAttachment::AttachType typeForAllAttachments, PassSettings passSettings = PassSettings::DefaultSubpassSettings());

    RenderpassAttachment& AddAttachment(const char* name, AttachmentFormat format);
    RenderpassAttachment& GetAttachment(const char* name);

    RenderpassAttachment& AddOutputAttachment();
};

struct RenderPipeline
{
    std::vector<Renderpass*> passes;
    PerfData perfData;

    Renderpass& AddPass(const char* name, PassSettings passSettings = PassSettings::DefaultRenderpassSettings());
    Renderpass& AddOutputPass();

    // Configurues all attachment in the order they are attached to the pipeline
    bool ConfigureAttachments();
    bool ConfigureAttachments(Renderpass& pass);

    // TODO: a horrible place to put this
    int dummyTextureUnit;
    static RenderpassAttachment& DummyAttachment();
};
