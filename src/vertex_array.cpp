#include <stdio.h>
#include <assert.h>

#include "vertex_array.h"

VertexArray::VertexArray()
{
    GLenum err;
    while((err = glGetError()) != GL_NO_ERROR)
    {
        printf("[pre gl gen vertex arrays] OpenGL error: %04x\n", err);
        assert(false);
    }

    glGenVertexArrays(1, &id);

    while((err = glGetError()) != GL_NO_ERROR)
        printf("[post gl gen vertex arrays] OpenGL error: %04x\n", err);

    Bind();
    while((err = glGetError()) != GL_NO_ERROR)
        printf("[post bind] OpenGL error: %04x\n", err);
}

VertexArray::VertexArray(VertexBuffer vertexBuffer, IndexBuffer indexBuffer,
        BufferLayout bufferLayout) 
    : VertexArray()
{
    GLenum err;

    AddVertexBuffer(vertexBuffer);
    while((err = glGetError()) != GL_NO_ERROR)
        printf("[post add vertex buffer] OpenGL error: %04x\n", err);

    SetIndexBuffer(indexBuffer);
    while((err = glGetError()) != GL_NO_ERROR)
        printf("[post set index buffer] OpenGL error: %04x\n", err);

    SetBufferLayout(bufferLayout);
    while((err = glGetError()) != GL_NO_ERROR)
        printf("[post set buffer layout] OpenGL error: %04x\n", err);

}

void VertexArray::Bind() const 
{
    glBindVertexArray(id);
}

void VertexArray::Unbind() const
{
    glBindVertexArray(0);
}

void VertexArray::AddVertexBuffer(VertexBuffer vertexBuffer) 
{
    Bind();
    vertexBuffer.Bind();

    vertexBuffers.push_back(vertexBuffer);
}

void VertexArray::SetIndexBuffer(IndexBuffer indexBuffer) 
{
    Bind();
    indexBuffer.Bind();

    this->indexBuffer = indexBuffer;
}

void VertexArray::SetBufferLayout(BufferLayout bufferLayout)
{
    Bind();

    int attributeIndex = 0;
    for (BufferLayout::ConstIterator it = bufferLayout.Begin(); it != bufferLayout.End(); it++) 
    {
        glEnableVertexArrayAttrib(id, attributeIndex);
        glVertexAttribPointer(attributeIndex, it->vectorElementCount, it->glType,
                it->normalized ? GL_TRUE : GL_FALSE, bufferLayout.Stride(),
                (void*) bufferLayout.Offset(attributeIndex));

        /*
        printf("[%s] index=%d, element count=%d, stride=%d, offset=%d\n", it->attributeName,
                attributeIndex, it->vectorElementCount, bufferLayout.Stride(),
                bufferLayout.Offset(attributeIndex));
        */

        attributeIndex++;
    }
}

int VertexArray::GetIndexCount() const
{
    return indexBuffer.Size();
}
