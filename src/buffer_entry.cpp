#include "buffer_entry.h"

BufferEntry::BufferEntry(const char *attributeName, int vectorElementCount, GLenum glType,
        int vectorElementSize, bool normalized)
    : attributeName(attributeName), vectorElementCount(vectorElementCount), glType(glType), 
      vectorElementSize(vectorElementSize), normalized(normalized),
      size(vectorElementCount * vectorElementSize)
{
}
