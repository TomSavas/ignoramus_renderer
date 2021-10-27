#include <string.h>
#include <GL/glew.h>

#include "log.h"

void GLLog(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
        const GLchar* message, const void* userParam)
{
    char sourceStr[32];
    switch (source)
    {
        case GL_DEBUG_SOURCE_API:
            strcpy(sourceStr, "api");
            break;
        case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
            strcpy(sourceStr, "window system");
            break;
        case GL_DEBUG_SOURCE_SHADER_COMPILER:
            strcpy(sourceStr, "shader compiler");
            break;
        case GL_DEBUG_SOURCE_THIRD_PARTY:
            strcpy(sourceStr, "3rd party");
            break;
        case GL_DEBUG_SOURCE_APPLICATION:
            strcpy(sourceStr, "app");
            break;
        case GL_DEBUG_SOURCE_OTHER:
        default:
            strcpy(sourceStr, "other");
            break;
    }
    
    char typeStr[32];
    switch (type)
    {
        case GL_DEBUG_TYPE_ERROR:
            strcpy(typeStr, "error");
            break;
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
            strcpy(typeStr, "deprecated behavior");
            break;
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
            strcpy(typeStr, "undefined behavior");
            break;
        case GL_DEBUG_TYPE_PORTABILITY:
            strcpy(typeStr, "portability");
            break;
        case GL_DEBUG_TYPE_PERFORMANCE:
            strcpy(typeStr, "performance");
            break;
        case GL_DEBUG_TYPE_MARKER:
            strcpy(typeStr, "marker");
            break;
        case GL_DEBUG_TYPE_PUSH_GROUP:
            strcpy(typeStr, "push group");
            break;
        case GL_DEBUG_TYPE_POP_GROUP:
            strcpy(typeStr, "pop group");
            break;
        case GL_DEBUG_TYPE_OTHER:
        default:
            strcpy(typeStr, "other");
            break;
    }

    switch (severity)
    {
        case GL_DEBUG_SEVERITY_HIGH:
            LOG_ERROR("OpenGL", "source: %s, type: %s, message:\n\t%s", sourceStr, typeStr, message);
            break;
        case GL_DEBUG_SEVERITY_MEDIUM:
            LOG_WARN("OpenGL", "source: %s, type: %s, message:\n\t%s", sourceStr, typeStr, message);
            break;
        case GL_DEBUG_SEVERITY_LOW:
            LOG_INFO("OpenGL", "source: %s, type: %s, message:\n\t%s", sourceStr, typeStr, message);
            break;
        case GL_DEBUG_SEVERITY_NOTIFICATION:
        default:
            LOG_DEBUG("OpenGL", "source: %s, type: %s, message:\n\t%s", sourceStr, typeStr, message);
            break;
    }
}
