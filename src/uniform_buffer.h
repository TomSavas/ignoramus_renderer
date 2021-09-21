#pragma once

struct UniformBuffer
{
    unsigned int id;
    unsigned int lastbindingIndex;

    UniformBuffer();

    Bind();
    Bind(unsigned int bindingIndex);

    UpdateUniform(size_t size, void* data);
};
