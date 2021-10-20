#include "render_pipeline.h"

#include <unordered_map>

#include "log.h"

GLenum ToGLInternalFormat(AttachmentFormat format)
{
    switch (format)
    {
        case AttachmentFormat::FLOAT_1:
            return GL_R32F;
        case AttachmentFormat::FLOAT_3:
            return GL_RGB16F;
        case AttachmentFormat::FLOAT_4:
            return GL_RGBA32F;
        case AttachmentFormat::DEPTH:
            return GL_DEPTH_COMPONENT32F;
        case AttachmentFormat::DEPTH_STENCIL:
            return GL_DEPTH24_STENCIL8;
        default:
            LOG_ERROR(__func__, "Cannot convert AttachmentFormat to GL internal format. Invalid format: %d", format);
            return GL_RGBA32F;
    }
}

GLenum ToGLFormat(AttachmentFormat format)
{
    switch (format)
    {
        case AttachmentFormat::FLOAT_1:
            return GL_RED;
        case AttachmentFormat::FLOAT_3:
            return GL_RGB;
        case AttachmentFormat::FLOAT_4:
            return GL_RGBA;
        case AttachmentFormat::DEPTH:
            return GL_DEPTH_COMPONENT;
        case AttachmentFormat::DEPTH_STENCIL:
            return GL_DEPTH_STENCIL;
        default:
            LOG_ERROR(__func__, "Cannot convert AttachmentFormat to GL format. Invalid format: %d", format);
            return GL_RGBA;
    }
}

GLenum ToGLType(AttachmentFormat format)
{
    switch (format)
    {
        case AttachmentFormat::FLOAT_1:
        case AttachmentFormat::FLOAT_3:
        case AttachmentFormat::FLOAT_4:
        case AttachmentFormat::DEPTH:
            return GL_FLOAT;
        case AttachmentFormat::DEPTH_STENCIL:
            return GL_UNSIGNED_INT_24_8;
        default:
            LOG_ERROR(__func__, "Cannot convert AttachmentFormat to GL type. Invalid format: %d", format);
            return GL_FLOAT;
    }
}

void PassSettings::Clear()
{
    if (ignoreClear)
        return;

    glClearColor(clearColor.x, clearColor.y, clearColor.z, clearColor.a);
    glClear(clear);
}

void PassSettings::Apply()
{
    if (ignoreApplication)
        return;

    for (int j = 0; j < enable.size(); j++)
        glEnable(enable[j]);
    glCullFace(cullFace);
    glDepthMask(depthMask);
    glDepthFunc(depthFunc);
    glBlendFunc(srcBlendFactor, dstBlendFactor);
    // missing color masking
}

void PassSettings::Apply(PassSettings& previousSettings)
{
    if (ignoreApplication || memcmp(&previousSettings, this, sizeof(PassSettings)) == 0)
    {
        return;
    }
}

/*static*/ PassSettings PassSettings::DefaultSettings()
{
    PassSettings settings;

    settings.ignoreApplication = false;
    settings.ignoreClear = false;

    settings.enable = { GL_DEPTH_TEST, GL_CULL_FACE };
    settings.cullFace = GL_BACK;
    settings.depthMask = GL_TRUE;
    settings.depthFunc = GL_LESS;
    settings.clearColor = glm::vec4(1.f, 1.f, 1.f, 1.f);
    for (int i = 0; i < 4; i++)
        settings.colorMask[i] = GL_TRUE;
    settings.clear = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT;
    settings.srcBlendFactor = GL_SRC_ALPHA;
    settings.dstBlendFactor = GL_ONE_MINUS_SRC_ALPHA;

    return settings;
}

/*static*/ PassSettings PassSettings::DefaultRenderpassSettings()
{
    return PassSettings::DefaultSettings();
}

/*static*/ PassSettings PassSettings::DefaultSubpassSettings()
{
    PassSettings settings = PassSettings::DefaultSettings();
    settings.ignoreApplication = true;
    settings.ignoreClear = true;

    return settings;
}

/*static*/ PassSettings PassSettings::DefaultOutputRenderpassSettings()
{
    return PassSettings::DefaultSettings();
}

// -------------------------------------------------------------------------------------------------

Renderpass& RenderPipeline::AddPass(const char* name, PassSettings passSettings)
{
    // TODO: Check if a pass with the same name extists already
    Renderpass* pass = new Renderpass(name, passSettings);
    passes.push_back(pass);

    return *pass;
}

Renderpass& RenderPipeline::AddOutputPass(Shader& screenQuadShader)
{
    Renderpass& outputPass = AddPass("output", PassSettings::DefaultOutputRenderpassSettings());
    outputPass.fbo = 0;

    // We can probably change the SubpassAttachment to make this nicer. But for the time being we only need to access
    // outputs from the previous valid pass
    RenderpassAttachment* previousValidOutputAttachment = nullptr;
    for (int i = passes.size() - 2; i >= 0 && previousValidOutputAttachment == nullptr; i--)
    {
        previousValidOutputAttachment = passes[i]->outputAttachment;
    }
    ASSERT(previousValidOutputAttachment != nullptr);

    Subpass& subpass = outputPass.AddSubpass("output subpass", &screenQuadShader, SCREEN_QUAD,
        {
            // Default framebuffer already has a color attachment, no need to add another one
            SubpassAttachment(previousValidOutputAttachment, SubpassAttachment::AS_TEXTURE, "tex")
        });

    return outputPass;
}

bool RenderPipeline::ConfigureAttachments()
{
    // Activating dummy texture and just letting it sit here forever
    {
        int maxTexUnits = 80;
        glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &maxTexUnits);
        dummyTextureUnit = maxTexUnits - 1;
        LOG_INFO("Render pipeline", "Max texture units: %d. Using texture unit #%d for dummy texture." , maxTexUnits, dummyTextureUnit);

        Texture dummyTexture("../assets/textures/dummy_black.png", true);
        dummyTexture.Activate(GL_TEXTURE0 + dummyTextureUnit);
    }

    for (auto* renderpass : passes)
    {
        ASSERT(renderpass != nullptr);
        if (!ConfigureAttachments(*renderpass))
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            return false;
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

bool RenderPipeline::ConfigureAttachments(Renderpass& pass)
{
    if (pass.fbo == 0)
    {
        // TODO: check if there are any COLOR attachments and report errors - not allowed that
        return true;
    }

    glGenFramebuffers(1, &pass.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, pass.fbo);

    int maxColorAttachments;
    glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &maxColorAttachments);

    // TODO: this can be reduced
    std::unordered_set<RenderpassAttachment*> renderpassColorAttachments;
    for (auto* subpass : pass.subpasses)
    {
        for (auto& subpassAttachment : subpass->attachments)
        {
            if (subpassAttachment.type == SubpassAttachment::AS_COLOR)
            {
                renderpassColorAttachments.insert(subpassAttachment.renderpassAttachment);
            }
        }
    }
    // TODO: for now let's just kill the program. Evevntually we can do something smarter and create multiple FBOs
    // but for now it's just a waste of time to do so
    ASSERT(renderpassColorAttachments.size() <= maxColorAttachments);

    // For now everything's a texture. Later on we can separate things into textures and renderbuffers
    std::vector<unsigned int> texIds(pass.attachments.size());
    glGenTextures(texIds.size(), texIds.data());
    for (int i = 0; i < pass.attachments.size(); i++)
    {
        ASSERT(pass.attachments[i] != nullptr);
        RenderpassAttachment& attachment = *pass.attachments[i];
        attachment.id = texIds[i];

        // Again, everything's a texture for now, no cubemaps or anything
        glBindTexture(GL_TEXTURE_2D, attachment.id);
        // TODO: fix hardcoded resolution and parameters
        glTexImage2D(GL_TEXTURE_2D, 0, ToGLInternalFormat(attachment.format), 1920, 1080, 0, ToGLFormat(attachment.format), ToGLType(attachment.format), NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        if (attachment.format == AttachmentFormat::DEPTH)
        {
            glTexParameteri(GL_TEXTURE_2D, GL_DEPTH_STENCIL_TEXTURE_MODE, GL_DEPTH_COMPONENT);
        }
    }

    pass.colorAttachmentIndices = std::vector<GLenum>();
    for (auto* subpass : pass.subpasses)
    {
        for (auto& subpassAttachment : subpass->attachments)
        {
            // All the other attachment types are handled during runtime
            if (subpassAttachment.type != SubpassAttachment::AS_COLOR)
                continue;

            GLenum currentColorAttachmentIndex = GL_COLOR_ATTACHMENT0 + pass.colorAttachmentIndices.size();
            glFramebufferTexture2D(GL_FRAMEBUFFER, currentColorAttachmentIndex, GL_TEXTURE_2D, subpassAttachment.renderpassAttachment->id, 0);

            subpass->colorAttachmentsToActivate.push_back(currentColorAttachmentIndex);
            pass.colorAttachmentIndices.push_back(currentColorAttachmentIndex);
        }
    }

    GLenum fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (fboStatus != GL_FRAMEBUFFER_COMPLETE)
    {
        LOG_ERROR("Render pipeline", "%s incomplete! Reason: %0x", pass.name, fboStatus);
        FORCE_BREAK;
        return false;
    }

    return true;
    // TODO: set the depth buffer here as well IFF we have only one of those. If we have many, we will be swapping them out at runtime
}

/*static*/ RenderpassAttachment& RenderPipeline::DummyAttachment()
{
    static RenderpassAttachment* dummyAttachment = nullptr;

    if (dummyAttachment == nullptr)
    {
        dummyAttachment = new RenderpassAttachment("dummy", AttachmentFormat::FLOAT_4);
        glGenTextures(1, &dummyAttachment->id);
        glBindTexture(GL_TEXTURE_2D, dummyAttachment->id);
        glTexImage2D(GL_TEXTURE_2D, 0, ToGLInternalFormat(dummyAttachment->format), 1920, 1080, 0, ToGLFormat(dummyAttachment->format), ToGLType(dummyAttachment->format), NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    return *dummyAttachment;
}

// -------------------------------------------------------------------------------------------------

Subpass& Renderpass::AddSubpass(const char* name, Shader* shader, MeshTag acceptedMeshTags, std::vector<SubpassAttachment> attachments, PassSettings passSettings)
{
    Subpass* subpass = new Subpass { name, shader, acceptedMeshTags, attachments, passSettings };
    subpasses.push_back(subpass);

    return *subpass;
}

Subpass& Renderpass::AddSubpass(const char* name, Shader* shader, MeshTag acceptedMeshTags, std::vector<RenderpassAttachment*> attachments, SubpassAttachment::AttachType typeForAllAttachments, PassSettings passSettings)
{
    std::vector<SubpassAttachment> subpassAttachments;
    for (int i = 0; i < attachments.size(); i++)
        subpassAttachments.push_back({ attachments[i], typeForAllAttachments });

    return AddSubpass(name, shader, acceptedMeshTags, subpassAttachments, passSettings);
}

RenderpassAttachment& Renderpass::AddAttachment(const char* name, AttachmentFormat format)
{
    RenderpassAttachment* attachment = new RenderpassAttachment(name, format);
    attachments.push_back(attachment);

    return *attachment;
}

RenderpassAttachment& Renderpass::GetAttachment(const char* attachmentName)
{
    for (auto* attachment : attachments)
    {
        if (strcmp(attachmentName, attachment->name) == 0)
            return *attachment;
    }

    LOG_ERROR("Render pipeline", "No attachment with name %s was found in renderpass %s. Providing dummy attachment", attachmentName, name);
    return RenderPipeline::DummyAttachment();
}

RenderpassAttachment& Renderpass::AddOutputAttachment()
{
    // Output pass cannot have an output attachment. Well it can and it does, but it's not an attachment we create, so
    // we can't access it. (Is that true..?)
    ASSERT(fbo != 0);

    // Always returns the same attachment. We can't have multiple output buffers.
    if (outputAttachment == nullptr)
    {
        char* outputAttachmentName = new char[256]; 
        sprintf(outputAttachmentName, "%s_output", name);
        outputAttachment = &AddAttachment(outputAttachmentName, AttachmentFormat::FLOAT_3);
    }

    return *outputAttachment;   
}

// -------------------------------------------------------------------------------------------------




















































///*static*/ Renderpass Renderpass::OutputPass(const char* outputAttachmentName)
//{
//    Renderpass pass = Renderpass("output pass", SCREEN_QUAD)
//        .RequestAttachmentFromPipeline({ outputAttachmentName, AS_TEXTURE, "tex" })
//        .AddSubpass(new Shader("../src/shaders/texture.vert", "../src/shaders/texture.frag"));
//    pass.isOutputPass = true;
//
//    return pass;
//}
//
//std::vector<const char*> Renderpass::OwnedAttachmentNames()
//{
//    std::vector<const char*> names;
//    for (auto& attachment : ownedAttachments)
//        names.push_back(attachment.name);
//
//    return names;
//}
//
//bool RenderPipeline::InitializePipeline()
//{
//    // Activating dummy texture and just letting it sit here forever
//    {
//        int maxTexUnits = 80;
//        glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &maxTexUnits);
//        dummyTextureUnit = maxTexUnits - 1;
//        LOG_INFO("Max texture units: %d. Using texture unit #%d for dummy texture." , maxTexUnits, dummyTextureUnit);
//
//        Texture dummyTexture("../assets/textures/dummy_black.png", true);
//        dummyTexture.Activate(GL_TEXTURE0 + dummyTextureUnit);
//    }
//
//    bool outputPassFound = false;
//
//    // TODO: rework this from for n do x, to a do X
//    for (Renderpass& pass : passes)
//    {
//        if (pass.isOutputPass)
//        {
//            pass.fbo = 0;
//            outputPassFound = true;
//        }
//        else
//        {
//            glGenFramebuffers(1, &pass.fbo);
//        }
//        glBindFramebuffer(GL_FRAMEBUFFER, pass.fbo);
//
//        // TODO: 8 buffers are guaranteed, might need more in the future though. Need to query system for max draw buffers in those cases
//        unsigned int colorAttachments[8];
//        unsigned int colorAttachmentCount = 0;
//
//        LOG_DEBUG("pass: %s", pass.name);
//        for (Attachment& attachment : pass.ownedAttachments)
//        {
//            attachment.owner = &pass;
//            LOG_DEBUG("owned attachment: %s", attachment.name);
//
//            switch (DeviseAttachmentPurpose(attachment))
//            {
//                case AttachmentPurpose::DEPTH:
//                case AttachmentPurpose::STENCIL:
//                case AttachmentPurpose::DEPTH_STENCIL:
//                    glGenRenderbuffers(1, &attachment.id);
//                    glBindRenderbuffer(GL_RENDERBUFFER, attachment.id);
//
//                    // TODO change to depth/stencil/depth_stencil
//                    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 1920, 1080);
//                    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, attachment.id);
//                    break;
//                default:
//                    glGenTextures(1, &attachment.id);
//                    glBindTexture(GL_TEXTURE_2D, attachment.id);
//
//                    // TODO: separate params for this
//                    if (attachment.format == AttachmentFormat::DEPTH)
//                    {
//                        if (attachment.purpose == AttachmentPurpose::DEPTH)
//                        {
//                            glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32, 1920, 1080, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
//                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
//                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
//                            // potentially some more settings
//                        }
//                        else if (attachment.purpose == AttachmentPurpose::COLOR)
//                        {
//                            // NOTE: THIS IS COMPLETELY FUCKED. How do we bind a "depth" texture for reading?
//                            //glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32, 1920, 1080, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, NULL);
//                            glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, 1920, 1080, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
//                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
//                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
//
//                            break;
//                        }
//                    }
//                    else
//                    {
//                        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 1920, 1080, 0, GL_RGBA, GL_FLOAT, NULL);
//                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
//                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
//                    }
//
//                    if (attachment.purpose == AttachmentPurpose::DEPTH)
//                    {
//                        LOG_DEBUG("added as depth");
//                        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, attachment.id, 0);
//                    }
//                    else
//                    {
//                        attachment.colorAttachmentIndex = colorAttachmentCount++;
//                        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + attachment.colorAttachmentIndex, GL_TEXTURE_2D, attachment.id, 0);
//                        colorAttachments[attachment.colorAttachmentIndex] = GL_COLOR_ATTACHMENT0 + attachment.colorAttachmentIndex;
//                        LOG_DEBUG("added as color");
//                    }
//
//                    break;
//            }
//            LOG_DEBUG("------------------------");
//            attachments.emplace(attachment.name, &attachment);
//        }
//
//        for (int i = 0; i < pass.requestedAttachments.size(); ++i)
//        {
//            //if (pass.requestedAttachments[i].requestAs != AS_ATTACHMENT)
//            // TEMP
//            if (pass.requestedAttachments[i].requestAs != AS_ATTACHMENT && pass.requestedAttachments[i].requestAs != AS_DEPTH)
//                continue;
//
//            auto attachmentIt = attachments.find(pass.requestedAttachments[i].name);
//            if (attachmentIt == attachments.end())
//            {
//                ASSERTF("Attachment \"%s\" has not been previously initialized in the pipeline!\n", pass.requestedAttachments[i].name);
//                return false;
//            }
//            Attachment* attachment = attachmentIt->second;
//            assert(attachment != nullptr);
//
//            //LOG_DEBUG("requested attachment: %s", attachment->name);
//
//            // TEMP
//            if (pass.requestedAttachments[i].requestAs == AS_DEPTH)
//            {
//                    glBindTexture(GL_TEXTURE_2D, attachment->id);
//
//                    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32, 1920, 1080, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
//                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
//                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
//
//                    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, attachment->id, 0);
//                    //LOG_DEBUG("added as depth");
//                    continue;
//            }
//
//            switch (DeviseAttachmentPurpose(*attachment))
//            {
//                case AttachmentPurpose::DEPTH:
//                case AttachmentPurpose::STENCIL:
//                case AttachmentPurpose::DEPTH_STENCIL:
//                    // TODO: is this required?
//                    glBindRenderbuffer(GL_RENDERBUFFER, attachment->id);
//                    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, attachment->id);
//                    glBindRenderbuffer(GL_RENDERBUFFER, 0);
//                    break;
//                default:
//                    // TODO: is this required?
//                    //glBindTexture(GL_TEXTURE_2D, attachment->id);
//
//                    //attachment->colorAttachmentIndex = colorAttachmentCount++;
//                    //glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + attachment->colorAttachmentIndex, GL_TEXTURE_2D, attachment->id, 0);
//                    //glBindTexture(GL_TEXTURE_2D, 0);
//                    //colorAttachments[attachment->colorAttachmentIndex] = GL_COLOR_ATTACHMENT0 + attachment->colorAttachmentIndex;
//
//                    glBindTexture(GL_TEXTURE_2D, attachment->id);
//
//                    // TODO: separate params for this
//                    if (attachment->format == AttachmentFormat::DEPTH)
//                    {
//                        if (attachment->purpose == AttachmentPurpose::DEPTH)
//                        {
//                            glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32, 1920, 1080, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
//                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
//                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
//                            // potentially some more settings
//                        }
//                        else if (attachment->purpose == AttachmentPurpose::COLOR)
//                        {
//                            glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32, 1920, 1080, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
//                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
//                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
//                        }
//                    }
//                    else
//                    {
//                        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 1920, 1080, 0, GL_RGBA, GL_FLOAT, NULL);
//                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
//                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
//                    }
//
//                    if (attachment->purpose == AttachmentPurpose::DEPTH)
//                    {
//                        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, attachment->id, 0);
//                    }
//                    else
//                    {
//                        attachment->colorAttachmentIndex = colorAttachmentCount++;
//                        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + attachment->colorAttachmentIndex, GL_TEXTURE_2D, attachment->id, 0);
//                        colorAttachments[attachment->colorAttachmentIndex] = GL_COLOR_ATTACHMENT0 + attachment->colorAttachmentIndex;
//                    }
//
//                    break;
//            }
//
//            //LOG_DEBUG("------------------------");
//        }
//
//        // TODO: oversized array should be fine?
//        assert(!pass.isOutputPass || (pass.isOutputPass && colorAttachmentCount == 0));
//        if (colorAttachmentCount > 0)
//            glDrawBuffers(colorAttachmentCount, colorAttachments);
//
//        GLenum fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
//        if (fboStatus != GL_FRAMEBUFFER_COMPLETE)
//        {
//            LOG_ERROR("Framebuffer incomplete! Reason: %0x", fboStatus);
//            assert(false);
//        }
//    }
//
//    glBindFramebuffer(GL_FRAMEBUFFER, 0);
//    assert(outputPassFound);
//
//    return true;
//}
//
//Attachment* RenderPipeline::RetrieveAttachment(const char* attachmentName) 
//{
//   auto attachment = attachments.find(attachmentName);
//
//   if (attachment == attachments.end())
//       return nullptr;
//
//    return attachment->second;
//}
