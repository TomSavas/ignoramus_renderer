#ifndef INDEX_BUFFER_H
#define INDEX_BUFFER_H

class IndexBuffer
{
public:
    IndexBuffer();
    IndexBuffer(unsigned int *indices, int size);
    IndexBuffer(unsigned int *indices, int size, unsigned int id);

    void Bind() const;
    void Unbind() const;

    int Size() const;

private:
    unsigned int id;
    int size;
    bool loaded;
};

#endif
