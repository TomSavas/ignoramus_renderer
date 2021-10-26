#include <stdio.h>
#include <assert.h>

#include "vertex_array.h"

VertexArray::VertexArray()
{
    glGenVertexArrays(1, &id);
    Bind();
}

VertexArray::VertexArray(VertexBuffer vertexBuffer, IndexBuffer indexBuffer,
        BufferLayout bufferLayout) 
    : VertexArray()
{
    AddVertexBuffer(vertexBuffer);
    SetIndexBuffer(indexBuffer);
    SetBufferLayout(bufferLayout);
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
