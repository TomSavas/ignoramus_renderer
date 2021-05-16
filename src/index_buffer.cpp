#include <cassert>
#include <stdio.h>

#include <GL/glew.h>

#include "index_buffer.h"

IndexBuffer::IndexBuffer()
    : size(0), loaded(false)
{
}

IndexBuffer::IndexBuffer(unsigned int *indices, int size)
    : size(size), loaded(true)
{
    glGenBuffers(1, &id);
    Bind();
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * size, indices, GL_STATIC_DRAW);

    GLenum err;
    while((err = glGetError()) != GL_NO_ERROR)
        printf("[post index buffer buffer data] OpenGL error: %04x\n", err);
}

IndexBuffer::IndexBuffer(unsigned int *indices, int size, unsigned int id)
    : id(id), size(size), loaded(true)
{
    Bind();
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * size, indices, GL_STATIC_DRAW);
}


void IndexBuffer::Bind() const
{
    assert(("Index buffer is not loaded", loaded));

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, id);
}

void IndexBuffer::Unbind() const
{
    assert(("Index buffer is not loaded", loaded));

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

int IndexBuffer::Size() const
{
    return size;
}
