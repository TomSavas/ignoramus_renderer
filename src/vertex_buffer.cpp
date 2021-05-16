#include <cassert>
#include <stdio.h>

#include <GL/glew.h>

#include "vertex_buffer.h"

VertexBuffer::VertexBuffer() 
    : size(0), loaded(false)
{
}

VertexBuffer::VertexBuffer(float *vertices, int size)
    : size(size), loaded(true)
{
    glGenBuffers(1, &id);
    GLenum err;
    while((err = glGetError()) != GL_NO_ERROR)
        printf("[post vertex buffer gen buffers] OpenGL error: %04x\n", err);
    Bind();
    while((err = glGetError()) != GL_NO_ERROR)
        printf("[post vertex buffer bind] OpenGL error: %04x\n", err);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * size, vertices, GL_STATIC_DRAW);

    while((err = glGetError()) != GL_NO_ERROR)
        printf("[post vertex buffer buffer data] OpenGL error: %04x\n", err);
}

VertexBuffer::VertexBuffer(float *vertices, int size, unsigned int id)
    : id(id), size(size), loaded(true)
{
    Bind();
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * size, vertices, GL_STATIC_DRAW);
}


void VertexBuffer::Bind() const
{
    assert(("Vertex buffer is not loaded", loaded));

    glBindBuffer(GL_ARRAY_BUFFER, id);
}

void VertexBuffer::Unbind() const
{
    assert(("Vertex buffer is not loaded", loaded));

    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

int VertexBuffer::Size() const
{
    return size;
}
