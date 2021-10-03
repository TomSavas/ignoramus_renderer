#include "material.h"

Material::Material(unsigned int structSize, unsigned int materialId, void *data)
{
    nonResourceData.size = structSize;
    nonResourceData.materialId = materialId;
    nonResourceData.data = data;

    glGenBuffers(1, &nonResourceData.uboId);
    glBindBufferBase(GL_UNIFORM_BUFFER, Shader::materialParamBindingPoint, nonResourceData.uboId);
    UpdateData();
}

void Material::UpdateData()
{
    // TODO: maybe ignore hwen size == 0
    glBindBuffer(GL_UNIFORM_BUFFER, nonResourceData.uboId);
    glBufferData(GL_UNIFORM_BUFFER, nonResourceData.size, nonResourceData.data, GL_STATIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void Material::Bind()
{
    glBindBufferBase(GL_UNIFORM_BUFFER, Shader::materialParamBindingPoint, nonResourceData.uboId);
}
