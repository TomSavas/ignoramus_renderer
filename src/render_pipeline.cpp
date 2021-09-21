#include "render_pipeline.h"

#include <unordered_map>

#include "log.h"
/*static*/ Renderpass Renderpass::OutputPass(const char* outputAttachmentName)
{
    Renderpass pass = Renderpass("output pass", SCREEN_QUAD)
        .RequestAttachmentFromPipeline({ outputAttachmentName, AS_TEXTURE, "tex" })
        .AddSubpass(new Shader("../src/shaders/texture.vert", "../src/shaders/texture.frag"));
    pass.isOutputPass = true;

    return pass;
}

std::vector<const char*> Renderpass::OwnedAttachmentNames()
{
    std::vector<const char*> names;
    for (auto& attachment : ownedAttachments)
        names.push_back(attachment.name);

    return names;
}

bool RenderPipeline::InitializePipeline()
{
    // Activating dummy texture and just letting it sit here forever
    {
        int maxTexUnits = 80;
        glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &maxTexUnits);
        dummyTextureUnit = maxTexUnits - 1;
        LOG_INFO("Max texture units: %d. Using texture unit #%d for dummy texture." , maxTexUnits, dummyTextureUnit);

        Texture dummyTexture("../assets/textures/dummy_black.png", true);
        dummyTexture.Activate(GL_TEXTURE0 + dummyTextureUnit);
    }

    bool outputPassFound = false;

    for (Renderpass& pass : passes)
    {
        if (pass.isOutputPass)
        {
            pass.fbo = 0;
            outputPassFound = true;
        }
        else
        {
            glGenFramebuffers(1, &pass.fbo);
        }
        glBindFramebuffer(GL_FRAMEBUFFER, pass.fbo);

        // TODO: 8 buffers are guaranteed, might need more in the future though. Need to query system for max draw buffers in those cases
        unsigned int colorAttachments[8];
        unsigned int colorAttachmentCount = 0;

        for (Attachment& attachment : pass.ownedAttachments)
        {
            // If we're using a read-write depth/stencil attachment, it's a texture
            const bool depthStencilPurpose = attachment.purpose == AttachmentPurpose::DEPTH || 
                attachment.purpose == AttachmentPurpose::STENCIL || 
                attachment.purpose == AttachmentPurpose::DEPTH_STENCIL;
            AttachmentPurpose devisedPurpose = depthStencilPurpose && attachment.access == AttachmentAccess::READ_WRITE 
                ? AttachmentPurpose::COLOR 
                : attachment.purpose;

            switch (devisedPurpose)
            {
                case AttachmentPurpose::DEPTH:
                case AttachmentPurpose::STENCIL:
                case AttachmentPurpose::DEPTH_STENCIL:
                    glGenRenderbuffers(1, &attachment.id);
                    glBindRenderbuffer(GL_RENDERBUFFER, attachment.id);

                    // TODO change to depth/stencil/depth_stencil
                    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 1920, 1080);
                    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, attachment.id);
                    break;
                default:
                    glGenTextures(1, &attachment.id);
                    glBindTexture(GL_TEXTURE_2D, attachment.id);

                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 1920, 1080, 0, GL_RGBA, GL_FLOAT, NULL);

                    // TODO: separate params for this
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

                    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + colorAttachmentCount, GL_TEXTURE_2D, attachment.id, 0);
                    colorAttachments[colorAttachmentCount] = GL_COLOR_ATTACHMENT0 + colorAttachmentCount;
                    colorAttachmentCount++;
                    break;
            }
            attachments.emplace(attachment.name, &attachment);
        }

        for (int i = 0; i < pass.requestedAttachments.size(); ++i)
        {
            if (pass.requestedAttachments[i].requestAs != AS_ATTACHMENT)
                continue;

            auto attachmentIt = attachments.find(pass.requestedAttachments[i].name);
            //if (!attachments.contains(pass.requestedAttachments[i].name))
            if (attachmentIt == attachments.end())
            {
                ASSERTF("Attachment \"%s\" has not been previously initialized in the pipeline!\n", pass.requestedAttachments[i].name);
                return false;
            }
            Attachment* attachment = attachmentIt->second;

            const bool depthStencilPurpose = attachment->purpose == AttachmentPurpose::DEPTH || 
                attachment->purpose == AttachmentPurpose::STENCIL || 
                attachment->purpose == AttachmentPurpose::DEPTH_STENCIL;
            AttachmentPurpose devisedPurpose = depthStencilPurpose && attachment->access == AttachmentAccess::READ_WRITE 
                ? AttachmentPurpose::COLOR 
                : attachment->purpose;

            switch (devisedPurpose)
            {
                case AttachmentPurpose::DEPTH:
                case AttachmentPurpose::STENCIL:
                case AttachmentPurpose::DEPTH_STENCIL:
                    // TODO: is this required?
                    glBindRenderbuffer(GL_RENDERBUFFER, attachment->id);
                    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, attachment->id);
                    glBindRenderbuffer(GL_RENDERBUFFER, 0);
                    break;
                default:
                    // TODO: is this required?
                    glBindTexture(GL_TEXTURE_2D, attachment->id);
                    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + colorAttachmentCount, GL_TEXTURE_2D, attachment->id, 0);
                    glBindTexture(GL_TEXTURE_2D, 0);
                    colorAttachments[colorAttachmentCount] = GL_COLOR_ATTACHMENT0 + colorAttachmentCount;
                    colorAttachmentCount++;
                    break;
            }
        }

        // TODO: oversized array should be fine?
        assert(!pass.isOutputPass || (pass.isOutputPass && colorAttachmentCount == 0));
        if (colorAttachmentCount > 0)
            glDrawBuffers(colorAttachmentCount, colorAttachments);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    assert(outputPassFound);

    return true;
}

Attachment* RenderPipeline::RetrieveAttachment(const char* attachmentName) 
{
   auto attachment = attachments.find(attachmentName);

   if (attachment == attachments.end())
       return nullptr;

    return attachment->second;
}
