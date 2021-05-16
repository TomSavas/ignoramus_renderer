#ifndef VERTEX_BUFFER_H
#define VERTEX_BUFFER_H

class VertexBuffer
{
public:
    VertexBuffer();
    VertexBuffer(float *vertices, int size);
    VertexBuffer(float *vertices, int size, unsigned int id);

    void Bind() const;
    void Unbind() const;

    int Size() const;

private:
    unsigned int id;
    int size;
    bool loaded;
};

#endif
