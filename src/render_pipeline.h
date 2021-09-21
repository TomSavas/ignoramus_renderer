#include <vector>
#include <unordered_map>

#include "mesh.h"

enum class AttachmentAccess
{
    READ_WRITE = 0,
    WRITE,
};
enum class AttachmentFormat
{
    FLOAT_1 = 0,
    FLOAT_2,
    FLOAT_3,
    FLOAT_4,
    DEPTH,
    DEPTH_STENCIL,
};
enum class AttachmentPurpose
{
    DEPTH = 0,
    STENCIL,
    DEPTH_STENCIL,
    COLOR,
};

struct Attachment
{
    const char *name;

    AttachmentFormat format;
    AttachmentPurpose purpose;
    AttachmentAccess access;

    unsigned int id;
};

enum RequestAttachmentsAs
{
    AS_TEXTURE,
    AS_ATTACHMENT,
    AS_BLIT
};

struct RequestedAttachment
{
    const char* name;
    RequestAttachmentsAs requestAs;

    // Only if AS_BLIT
    const char* bindAs;
};

struct RenderpassSettings
{
    std::vector<GLenum> enable;   
    GLenum cullFace;

    GLboolean depthMask;

    glm::vec4 clearColor;
    GLenum clear;

    GLenum srcBlendFactor;
    GLenum dstBlendFactor;

    static RenderpassSettings Default()
    {
        RenderpassSettings settings;

        settings.enable = { GL_DEPTH_TEST, GL_CULL_FACE };
        settings.cullFace = GL_BACK;
        settings.depthMask = GL_TRUE;
        settings.clearColor = glm::vec4(1.f, 0.f, 0.f, 0.f);
        settings.clear = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT;
        settings.srcBlendFactor = GL_SRC_ALPHA;
        settings.dstBlendFactor = GL_ONE_MINUS_SRC_ALPHA;

        return settings;
    }
};

struct Renderpass
{
    const char *name;
    MeshTag acceptedMeshTags;

    std::vector<Attachment> ownedAttachments;
    std::vector<RequestedAttachment> requestedAttachments;

    std::vector<Shader*> subpasses;

    RenderpassSettings settings;

    unsigned int fbo;
    bool isOutputPass;

    Renderpass(const char* name, MeshTag tags, RenderpassSettings settings = RenderpassSettings::Default()) : name(name), acceptedMeshTags(tags), isOutputPass(false), settings(settings) {}
    static Renderpass OutputPass(const char* outputAttachmentName);

    inline Renderpass& AddAttachment(Attachment attachment) { ownedAttachments.push_back(attachment); return *this; }
    inline Renderpass& RequestAttachmentFromPipeline(RequestedAttachment attachment) { requestedAttachments.push_back(attachment); return *this; }
    inline Renderpass& AddSubpass(Shader* subpass) { subpasses.push_back(subpass); return *this; }

    std::vector<const char*> OwnedAttachmentNames();
};

struct RenderPipeline
{
    std::vector<Renderpass> passes;
    std::unordered_map<const char*, Attachment*> attachments;

    // TODO: a horrible place to put this
    int dummyTextureUnit;

    bool InitializePipeline();
    Attachment* RetrieveAttachment(const char* attachmentName);
};
