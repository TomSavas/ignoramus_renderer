#include "buffer_layout.h"

BufferLayout::BufferLayout()
    : BufferLayout({})
{
}

BufferLayout::BufferLayout(std::initializer_list<BufferEntry> entries)
    : entries(entries)
{
    int cumulativeOffset = 0;
    for (std::vector<BufferEntry>::const_iterator it = Begin(); it != End(); it++) 
    {
        offsets.push_back(cumulativeOffset);
        cumulativeOffset += it->vectorElementCount * it->vectorElementSize;
    }

    stride = cumulativeOffset;
}

int BufferLayout::Stride() const 
{
    return stride;
}

int BufferLayout::Offset(int index) const
{
    return offsets[index];
}

std::vector<BufferEntry>::const_iterator BufferLayout::Begin() const
{
    return entries.cbegin();
}

std::vector<BufferEntry>::const_iterator BufferLayout::End() const
{
    return entries.cend();
}
