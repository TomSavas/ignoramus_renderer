#ifndef BUFFER_ENTRY_H
#define BUFFER_ENTRY_H

#include <GL/glew.h>

struct BufferEntry
{
    const char *attributeName;
    int vectorElementCount;
    GLenum glType;
    int vectorElementSize;
    bool normalized;
    int size;

    BufferEntry(const char *attributeName, int vectorSize, GLenum glType, int vectorElementSize,
            bool normalized = false);
};

#endif
