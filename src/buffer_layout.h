#ifndef BUFFER_LAYOUT_H
#define BUFFER_LAYOUT_H

#include <initializer_list>
#include <vector>

#include "buffer_entry.h"

class BufferLayout
{
public:
    using ConstIterator = std::vector<BufferEntry>::const_iterator;
    
    BufferLayout();
    BufferLayout(std::initializer_list<BufferEntry> entries);

    int Stride() const;
    int Offset(int index) const;

    ConstIterator Begin() const;
    ConstIterator End() const;

private:
    std::vector<BufferEntry> entries;

    int stride;
    std::vector<int> offsets;
};

#endif
