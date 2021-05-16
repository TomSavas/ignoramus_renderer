#ifndef VERTEX_ARRAY_H
#define VERTEX_ARRAY_H

#include <vector>

#include "buffer_layout.h"
#include "index_buffer.h"
#include "vertex_buffer.h"

class VertexArray
{
public:
    VertexArray();
    VertexArray(VertexBuffer vertexBuffer, IndexBuffer indexBuffer,
            BufferLayout bufferLayout);

    void Bind() const;
    void Unbind() const;

    void AddVertexBuffer(VertexBuffer vertexBuffer);
    void SetIndexBuffer(IndexBuffer indexBuffer);
    void SetBufferLayout(BufferLayout bufferLayout);

    int GetIndexCount() const;

private:
    std::vector<VertexBuffer> vertexBuffers;
    IndexBuffer indexBuffer;

    BufferLayout bufferLayout;

    unsigned int id;
};

#endif
