#pragma once

#include <GL/glew.h>
#include <unordered_map>
#include <string>

#include "shader.h"

struct Material
{
    struct 
    {
        unsigned int materialId;

        std::unordered_map<std::string, std::string> textures;
        unsigned int uboId;
        unsigned int size;
        void *data;
    } nonResourceData;

    void UpdateData();
    void Bind();

protected:
    Material(unsigned int structSize, unsigned int materialId, void *data);
};

//TODO: constexpr
static unsigned int globalIdentifiableIdCounter = 0;
template<typename T>
struct Identifiable
{
    static unsigned int Id() 
    {
        static unsigned int id = globalIdentifiableIdCounter++;
        return id;
    }
};

struct EmptyMaterial : public Material, public Identifiable<EmptyMaterial>
{
    EmptyMaterial() : Material(0, Id(), nullptr) {}
};

struct TransparentMaterial : public Material, public Identifiable<TransparentMaterial>
{
    struct
    {
        glm::vec4 tintAndOpacity;
    } resourceData;

    TransparentMaterial(float r, float g, float b, float opacity) : Material(sizeof(resourceData),
            Id(), &resourceData), resourceData({glm::vec3(r, g, b, opacity)}) {}
};

struct OpaqueMaterial : public Material, public Identifiable<OpaqueMaterial>
{
    struct
    {
    } resourceData;

    OpaqueMaterial() : Material(sizeof(resourceData), Id(), &resourceData) {}
};


/*
#define MATERIAL(name, structDef) \
struct name##Material : public Material, public Identifiable<name##Material> \
{ \
    structDef resourceData; \
    name##Material() : Material(sizeof(resourceData), Id(), &resourceData) {}\
};\
*/
