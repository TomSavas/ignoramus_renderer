#include "render_pipeline.h"

#include <unordered_map>

#include "log.h"
#include "hash.h"
#include "scene.h"

GLenum ToGLInternalFormat(AttachmentFormat format)
{
    switch (format)
    {
        case AttachmentFormat::UINT_1:
            return GL_R32UI;
        case AttachmentFormat::FLOAT_1:
            return GL_R32F;
        case AttachmentFormat::FLOAT_2:
            return GL_RG32F;
        case AttachmentFormat::FLOAT_3:
            return GL_RGB16F;
        case AttachmentFormat::FLOAT_4:
            return GL_RGBA32F;
        case AttachmentFormat::DEPTH:
            return GL_DEPTH_COMPONENT32F;
        case AttachmentFormat::DEPTH_STENCIL:
            return GL_DEPTH24_STENCIL8;
        case AttachmentFormat::SSBO:
            return GL_SHADER_STORAGE_BUFFER;
        case AttachmentFormat::ATOMIC_COUNTER:
            return GL_ATOMIC_COUNTER_BUFFER;
        default:
            LOG_ERROR(__func__, "Cannot convert AttachmentFormat to GL internal format. Invalid format: %d", format);
            return GL_RGBA32F;
    }
}

GLenum ToGLFormat(AttachmentFormat format)
{
    switch (format)
    {
        case AttachmentFormat::UINT_1:
            return GL_RED_INTEGER;
        case AttachmentFormat::FLOAT_1:
            return GL_RED;
        case AttachmentFormat::FLOAT_2:
            return GL_RG;
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
        case AttachmentFormat::UINT_1:
            return GL_UNSIGNED_INT;
        case AttachmentFormat::FLOAT_1:
        case AttachmentFormat::FLOAT_2:
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

int RenderpassAttachment::bufferCount = 0;
/*static*/ RenderpassAttachment RenderpassAttachment::SSBO(const char* name, long size)
{
    RenderpassAttachment attachment(name, AttachmentFormat::SSBO);
    attachment.size = size;
    attachment.bindingIndex = RenderpassAttachment::bufferCount++;

    return attachment;
}

/*static*/ RenderpassAttachment RenderpassAttachment::AtomicCounter(const char* name) 
{
    RenderpassAttachment attachment(name, AttachmentFormat::ATOMIC_COUNTER);
    attachment.size = sizeof(GLuint);
    attachment.bindingIndex = RenderpassAttachment::bufferCount++;

    return attachment;
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
    glBlendEquation(blendEquation);
    glBlendFunc(srcBlendFactor, dstBlendFactor);
    for (int i = 0; i < blendFactors.size(); i++)
    {
        glBlendFunci(i, blendFactors[i].srcFactor, blendFactors[i].dstFactor);
    }
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
    settings.blendEquation = GL_FUNC_ADD;
    settings.srcBlendFactor = GL_SRC_ALPHA;
    settings.dstBlendFactor = GL_ONE_MINUS_SRC_ALPHA;
    settings.computeWorkGroups = glm::ivec3(1, 1, 1);

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

static unsigned int clearBuffer;
RenderPipeline::RenderPipeline()
{
    glGenBuffers(1, &materialUbo);
    glGenQueries(1, &timeQuery);

    // TEMP
    std::vector<GLuint> headClear(1920 * 1080, 0xffffffff);
    glGenBuffers(1, &clearBuffer);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, clearBuffer);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, headClear.size() * sizeof(GLuint), headClear.data(), GL_STATIC_COPY);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}

Renderpass& RenderPipeline::AddPass(const char* name, PassSettings passSettings)
{
    // TODO: Check if a pass with the same name extists already
    Renderpass* pass = new Renderpass(name, passSettings);
    passes.push_back(pass);

    return *pass;
}

Renderpass& RenderPipeline::AddOutputPass(ShaderPool& shaders)
{
    Renderpass& outputPass = AddPass("Output pass", PassSettings::DefaultOutputRenderpassSettings());
    outputPass.fbo = 0;

    // We can probably change the SubpassAttachment to make this nicer. But for the time being we only need to access
    // outputs from the previous valid pass
    RenderpassAttachment* previousValidOutputAttachment = nullptr;
    for (int i = passes.size() - 2; i >= 0 && previousValidOutputAttachment == nullptr; i--)
    {
        previousValidOutputAttachment = passes[i]->outputAttachment;
    }
    ASSERT(previousValidOutputAttachment != nullptr);

    Subpass& subpass = outputPass.AddSubpass("output subpass", &shaders.GetShader(SCREEN_QUAD_TEXTURE_SHADER), SCREEN_QUAD,
        {
            // Default framebuffer already has a color attachment, no need to add another one
            SubpassAttachment(previousValidOutputAttachment, SubpassAttachment::AS_TEXTURE, "tex")
        });

    return outputPass;
}

bool ConfigureRenderpassAttachments(Renderpass& pass, bool validateFramebuffer)
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

    int textureCount = 0;
    int bufferCount = 0;
    for (int i = 0; i < pass.attachments.size(); i++)
    {
        ASSERT(pass.attachments[i] != nullptr);
        switch (pass.attachments[i]->format)
        {
            case AttachmentFormat::SSBO:
            case AttachmentFormat::ATOMIC_COUNTER:
                bufferCount++;
                break;
            default:
                textureCount++;
                break;
        }
    }

    int usedTextureCount = 0;
    std::vector<unsigned int> texIds(textureCount);
    glGenTextures(textureCount, texIds.data());

    int usedBufferCount = 0;
    std::vector<unsigned int> bufferIds(bufferCount);
    glGenBuffers(bufferCount, bufferIds.data());

    for (int i = 0; i < pass.attachments.size(); i++)
    {
        ASSERT(pass.attachments[i] != nullptr);
        RenderpassAttachment& attachment = *pass.attachments[i];

        switch (attachment.format)
        {
            case AttachmentFormat::SSBO:
            case AttachmentFormat::ATOMIC_COUNTER:
                attachment.id = bufferIds[usedBufferCount++];

                glBindBuffer(ToGLInternalFormat(attachment.format), attachment.id);
                // TODO: dynamic draw should be configurable...
                glBufferData(ToGLInternalFormat(attachment.format), attachment.size, NULL, GL_DYNAMIC_DRAW);
                break;
            default:
                attachment.id = texIds[usedTextureCount++];

                // Again, everything's a texture for now, no cubemaps or anything
                glBindTexture(GL_TEXTURE_2D, attachment.id);
                // TODO: fix hardcoded resolution and parameters
                glTexImage2D(GL_TEXTURE_2D, 0, ToGLInternalFormat(attachment.format), 1920, 1080, 0, ToGLFormat(attachment.format), ToGLType(attachment.format), NULL);

                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

                if (attachment.format == AttachmentFormat::DEPTH)
                {
                    glTexParameteri(GL_TEXTURE_2D, GL_DEPTH_STENCIL_TEXTURE_MODE, GL_DEPTH_COMPONENT);

                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

                    const float darkBorder[] = { 0.f, 0.f, 0.f, 0.f };
                    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, darkBorder);
                }
                break;
        }
    }

    pass.allColorAttachmentIndices = std::vector<GLenum>();
    std::unordered_set<RenderpassAttachment*> registeredAttachments;
    for (auto* subpass : pass.subpasses)
    {
        for (auto& subpassAttachment : subpass->attachments)
        {
            if (registeredAttachments.find(subpassAttachment.renderpassAttachment) != registeredAttachments.end())
            {
                subpass->colorAttachmentsToActivate.push_back(subpassAttachment.renderpassAttachment->attachmentIndex);
                continue;
            }

            // All the other attachment types are handled during runtime
            if (subpassAttachment.type != SubpassAttachment::AS_COLOR)
            {
                continue;
            }

            GLenum currentColorAttachmentIndex = GL_COLOR_ATTACHMENT0 + pass.allColorAttachmentIndices.size();
            subpassAttachment.renderpassAttachment->attachmentIndex = currentColorAttachmentIndex;
            glFramebufferTexture2D(GL_FRAMEBUFFER, currentColorAttachmentIndex, GL_TEXTURE_2D, subpassAttachment.renderpassAttachment->id, 0);

            subpass->colorAttachmentsToActivate.push_back(subpassAttachment.renderpassAttachment->attachmentIndex);

            pass.allColorAttachmentIndices.push_back(currentColorAttachmentIndex);
            registeredAttachments.insert(subpassAttachment.renderpassAttachment);
        }
    }

    if (!validateFramebuffer)
    {
        LOG_WARN("Render pipeline", "Skipping fb validation for \"%s\"", pass.name);
        return true;
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

bool RenderPipeline::ConfigureAttachments(bool validateFramebuffers)
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
        if (!ConfigureRenderpassAttachments(*renderpass, validateFramebuffers))
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            return false;
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
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
    return InsertSubpass(subpasses.size(), name, shader, acceptedMeshTags, attachments, passSettings);;
}

Subpass& Renderpass::InsertSubpass(int index, const char* name, Shader* shader, MeshTag acceptedMeshTags, std::vector<SubpassAttachment> attachments, PassSettings passSettings)
{
    Subpass* subpass = new Subpass { name, shader, acceptedMeshTags, attachments, passSettings };
    subpasses.insert(subpasses.begin() + index, subpass);

    return *subpass;
    
}

RenderpassAttachment& Renderpass::AddAttachment(RenderpassAttachment attachment)
{
    RenderpassAttachment* newAttachment = new RenderpassAttachment();
    *newAttachment = attachment;
    attachments.push_back(newAttachment);

    return *newAttachment;
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
        outputAttachment = &AddAttachment(RenderpassAttachment(outputAttachmentName, AttachmentFormat::FLOAT_4));
    }

    return *outputAttachment;   
}

void Renderpass::AddDefine(const char* define, const char* value)
{
    unsigned long hash = Djb2((const unsigned char*) define);
    defines[hash] = (ShaderDescriptor::Define) { define, value };
}

void Renderpass::AddDefine(const char* define, int value)
{
    // Mem leak, I know - too bad!
    char* valueStr = (char*) malloc(sizeof(char) * 8);
    sprintf(valueStr, "%d", value);
    AddDefine(define, valueStr);
}

std::vector<ShaderDescriptor::Define> Renderpass::DefineValues(std::vector<ShaderDescriptor::Define> existingDefines)
{
    for (auto& define : defines)
    {
        existingDefines.push_back(define.second);
    }
    return existingDefines;
}

void RenderPipeline::Render(Scene& scene, ShaderPool& shaders)
{
    // TODO: don't really need to do every frame
    scene.BindSceneParams();
    // TODO: support for multiple cameras
    scene.BindCameraParams();
    scene.BindLighting();

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, scene.globalAttachments.GetAttachment("PointLights").id);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(Scene::Lights), &scene.lights, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    glBindBufferBase(GL_UNIFORM_BUFFER, Shader::modelParamBindingPoint, materialUbo);

    float pipelineCpuDurationMs = 0.f;
    float pipelineGpuDurationMs = 0.f;
    int totalCulled = 0;

    static PassSettings previousSettings = PassSettings::DefaultSettings();
    for (int i = 0; i < passes.size(); i++)
    {
        int activatedTextureCount = 0;
        int activatedImageCount = 0;
        ASSERT(passes[i] != nullptr);
        Renderpass& renderpass = *passes[i];

        float renderpassCpuDurationMs = 0.f;
        float renderpassGpuDurationMs = 0.f;

        glBindFramebuffer(GL_FRAMEBUFFER, renderpass.fbo);

        // TODO: srsly?
        for (int j = 0; j < previousSettings.enable.size(); j++)
        {
            bool disable = true;
            for (int k = 0; k < renderpass.settings.enable.size(); k++)
            {
                if (previousSettings.enable[j] == renderpass.settings.enable[k])
                {
                    disable = false;
                    break;
                }
            }

            if (disable)
            {
                glDisable(previousSettings.enable[j]);
            }
        }
        if (renderpass.fbo != 0)
        {
            glDrawBuffers(renderpass.allColorAttachmentIndices.size(), renderpass.allColorAttachmentIndices.data());
        }
        renderpass.settings.Apply();
        renderpass.settings.Clear();
        previousSettings = renderpass.settings;

        // TODO: this duplicates clears and is generally pretty stupid
        {
            for (auto* attachment : renderpass.attachments)
            {
                if (attachment->hasSeparateClearOpts && attachment->attachmentIndex != INVALID_ATTACHMENT_INDEX)
                {
                    glDrawBuffers(1, &attachment->attachmentIndex);
                    glClearColor(attachment->clearOpts.color.x, attachment->clearOpts.color.y, attachment->clearOpts.color.z,
                            attachment->clearOpts.color.w);
                    glClear(GL_COLOR_BUFFER_BIT);
                }
            }
            if (renderpass.fbo != 0)
            {
                glDrawBuffers(renderpass.allColorAttachmentIndices.size(), renderpass.allColorAttachmentIndices.data());
            }
        }

        for (int j = 0; j < renderpass.subpasses.size(); j++)
        {
            ASSERT(renderpass.subpasses[j] != nullptr);
            Subpass& subpass = *renderpass.subpasses[j];

            // Maybe move this after all the clears?
            if (subpass.acceptedMeshTags == OPAQUE && scene.disableOpaque)
            {
                continue;
            }

            // TEMP
            if (strcmp(subpass.name, "Composition-lighting subpass") == 0 || strcmp(subpass.name, "sorting subpass") == 0)
            {
                glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
            }

            clock_t subpassStartTime = clock();
            glBeginQuery(GL_TIME_ELAPSED, timeQuery);

            subpass.shader->Use();

            for (SubpassAttachment& subpassAttachment : subpass.attachments)
            {
                ASSERT(subpassAttachment.renderpassAttachment != nullptr);

                unsigned int attachmentId = subpassAttachment.renderpassAttachment->id;
                int binding;
                switch (subpassAttachment.type)
                {
                    case SubpassAttachment::AS_COLOR:
                        continue;
                    case SubpassAttachment::AS_DEPTH:
                        // Remember, for the time being everything's a texture!
                        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, attachmentId, 0);
                        break;
                    case SubpassAttachment::AS_TEXTURE:
                        // TODO: Absolutely idiotic this. Should have a MRU eviction policy cache for texture units
                        // ... maybe MRU is not the best choice? No clue, needs testing
                        glActiveTexture(GL_TEXTURE0 + activatedTextureCount);
                        subpass.shader->SetUniform(subpassAttachment.useAs, activatedTextureCount++);
                        glBindTexture(GL_TEXTURE_2D, attachmentId);
                        break;
                    case SubpassAttachment::AS_IMAGE:
                        // TODO: add ability to make this read/write only

                        // TEMP clear
                        if (strcmp(subpass.name, "transparent geometry pass") == 0 || strcmp(subpass.name, "weighted blended transparency pass") == 0)
                        {
                            //LOG_DEBUG("clear", "clearing image");
                            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, clearBuffer);
                            glBindTexture(GL_TEXTURE_2D, attachmentId);
                            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1920, 1080, GL_RED_INTEGER, GL_UNSIGNED_INT, NULL);
                            glBindTexture(GL_TEXTURE_2D, 0);
                            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
                        }

                        //glBindImageTexture(activatedImageCount++, attachmentId, 0, GL_FALSE, 0, GL_READ_WRITE, ToGLInternalFormat(subpassAttachment.renderpassAttachment->format));
                        // NOTE: I think this is correct. Seems to work fine under all conditions. Not 100% if correct tho
                        binding = subpass.shader->GetBinding(subpassAttachment.useAs);
                        if (binding != INVALID_BINDING)
                        {
                            glBindImageTexture(binding, attachmentId, 0, GL_FALSE, 0, GL_READ_WRITE, ToGLInternalFormat(subpassAttachment.renderpassAttachment->format));
                        }
                        break;
                    case SubpassAttachment::AS_SSBO:

                        binding = subpass.shader->GetBinding(subpassAttachment.useAs);
                        if (binding != INVALID_BINDING)
                        {
                            if (strcmp(subpass.name, "Composition-lighting subpass") == 0)
                            {
                                //LOG_WARN("", "Binding %s to %d in %s", subpassAttachment.renderpassAttachment->name, binding, subpass.name);
                            }
                            //LOG_WARN("", "Binding %s to %d in %s", subpassAttachment.renderpassAttachment->name, binding, subpass.name);
                            glBindBufferBase(ToGLInternalFormat(subpassAttachment.renderpassAttachment->format), binding, attachmentId);
                        }
                        break;
                    case SubpassAttachment::AS_ATOMIC_COUNTER:
                        binding = subpass.shader->GetBinding(subpassAttachment.useAs);
                        if (binding != INVALID_BINDING)
                        {
                            glBindBufferBase(ToGLInternalFormat(subpassAttachment.renderpassAttachment->format), binding, attachmentId);
                        }

                        // TEMP clear
                        //if (strcmp(subpass.name, "transparent geometry pass") == 0)
                        {
                            //LOG_DEBUG("clear", "clearing counter");
                            static GLuint zero = 0;
                            glBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint), &zero);
                        }

                        break;
                    case SubpassAttachment::AS_BLIT:
                        ASSERT(false);
                        break;
                }

                // TODO: clears the secondary depth buffer. AttachmentClearOpts should be handling this
                if (strcmp(renderpass.name, "Depth peeling pass") == 0 && j == 0 && (subpassAttachment.useAs != nullptr && strcmp(subpassAttachment.useAs, "greater_depth") == 0))
                {
                    float clearValue = 0.f;
                    glClearTexImage(attachmentId, 0, GL_DEPTH_COMPONENT, GL_FLOAT, &clearValue);
                }
            }

            // TODO: seriously?
            for (int k = 0; k < previousSettings.enable.size(); k++)
            {
                bool disable = true;
                for (int l = 0; l < subpass.settings.enable.size(); l++)
                {
                    if (previousSettings.enable[k] == subpass.settings.enable[l])
                    {
                        disable = false;
                        break;
                    }
                }

                if (disable)
                {
                    glDisable(previousSettings.enable[k]);
                }
            }
            if (renderpass.fbo != 0)
            {
                glDrawBuffers(subpass.colorAttachmentsToActivate.size(), subpass.colorAttachmentsToActivate.data());
            }
            subpass.settings.Apply();
            subpass.settings.Clear();
            previousSettings = subpass.settings;
            // TODO: this duplicates clears and is generally pretty stupid
            {
                for (auto& attachment : subpass.attachments)
                {
                    if (attachment.hasSeparateClearOpts && attachment.renderpassAttachment->attachmentIndex != INVALID_ATTACHMENT_INDEX)
                    {
                        glDrawBuffers(1, &attachment.renderpassAttachment->attachmentIndex);
                        glClearColor(attachment.clearOpts.color.x, attachment.clearOpts.color.y, attachment.clearOpts.color.z,
                                attachment.clearOpts.color.w);
                        glClear(GL_COLOR_BUFFER_BIT);
                    }
                }
                if (renderpass.fbo != 0)
                {
                    glDrawBuffers(subpass.colorAttachmentsToActivate.size(), subpass.colorAttachmentsToActivate.data());
                }
            }

            subpass.shader->AddDummyForUnboundTextures(dummyTextureUnit);

            //glMemoryBarrier(GL_ALL_BARRIER_BITS);
            if (subpass.acceptedMeshTags == COMPUTE)
            {
                glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
                glMemoryBarrier(GL_ATOMIC_COUNTER_BARRIER_BIT);
                glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
                glDispatchCompute(subpass.settings.computeWorkGroups.x, subpass.settings.computeWorkGroups.y, subpass.settings.computeWorkGroups.z);
                glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
                glMemoryBarrier(GL_ATOMIC_COUNTER_BARRIER_BIT);
                glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
            }
            //glMemoryBarrier(GL_ALL_BARRIER_BITS);

            for (int i = 0; i < sizeof(subpass.acceptedMeshTags) * 8; i++)
            {
                MeshTag acceptedMeshTag = (MeshTag)((int)subpass.acceptedMeshTags & (1 << i));
                if (acceptedMeshTag == 0)
                {
                    continue;
                }

                for (MeshWithMaterial& meshWithMaterial : scene.meshes[acceptedMeshTag])
                {
                    if (meshWithMaterial.mesh.meshTag == PARTICLE && !scene.renderParticles)
                    {
                        continue;
                    }

                    glm::mat4 model = meshWithMaterial.mesh.transform.Model();
                    bool fullyOutsideViewFrustum = meshWithMaterial.mesh.aabbModelSpace.ViewFrustumIntersect(scene.camera.MVP(model));
                    if (fullyOutsideViewFrustum && subpass.acceptedMeshTags != SCREEN_QUAD)
                    {
                        totalCulled++;
                        continue;
                    }
                    
                    // TODO: not necessary if already bound
                    //       even if bound and changed can only partially update
                    meshWithMaterial.material->Bind();
                    
                    if (meshWithMaterial.mesh.meshTag == PARTICLE)
                    {
                        meshWithMaterial.material->UpdateData();
                    }

                    glBindBuffer(GL_UNIFORM_BUFFER, materialUbo);
                    glBufferData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), &model, GL_STATIC_DRAW);
                    //glBindBuffer(GL_UNIFORM_BUFFER, 0);

                    // TODO: Should be reworked - there should be some sort of communication between the shader and model here.
                    // Shader dictates what textures it needs, the model provides them
                    meshWithMaterial.mesh.Render(*subpass.shader);
                }
            }

            clock_t subpassEndTime = clock();
            clock_t subpassCpuDuration = subpassEndTime - subpassStartTime;
            float subpassCpuDurationMs = subpassCpuDuration * 1000.f / CLOCKS_PER_SEC;
            subpass.perfData.cpu.AddFrametime(subpassCpuDurationMs);
            renderpassCpuDurationMs += subpassCpuDurationMs;

            glEndQuery(GL_TIME_ELAPSED);
            bool queryDone = false;
            while (!queryDone) 
            {
                glGetQueryObjectiv(timeQuery, GL_QUERY_RESULT_AVAILABLE, (int*)&queryDone);
            }
            long subpassGpuDurationNs;
            glGetQueryObjecti64v(timeQuery, GL_QUERY_RESULT, &subpassGpuDurationNs);
            double subpassGpuDurationMs = (double)subpassGpuDurationNs / 1000000.0;
            renderpassGpuDurationMs += subpassGpuDurationMs;
            subpass.perfData.gpu.AddFrametime(subpassGpuDurationMs);
        }
        renderpass.perfData.cpu.AddFrametime(renderpassCpuDurationMs);
        renderpass.perfData.gpu.AddFrametime(renderpassGpuDurationMs);

        pipelineCpuDurationMs += renderpassCpuDurationMs;
        pipelineGpuDurationMs += renderpassGpuDurationMs;
    }
    perfData.cpu.AddFrametime(pipelineCpuDurationMs);
    perfData.gpu.AddFrametime(pipelineGpuDurationMs);
}
