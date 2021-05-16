#ifndef TEXTURE_H
#define TEXTURE_H

#include <GL/glew.h>

#include "glm/glm.hpp"

class Texture 
{
public:
    Texture(glm::ivec2 size = glm::ivec2(1), glm::vec2 scale = glm::vec2(1.f));
    Texture(const char *filepath, bool load = false, glm::vec2 scale = glm::vec2(1.f));
    Texture(unsigned int id, glm::ivec2 size = glm::ivec2(1), glm::vec2 scale = glm::vec2(1.f));
    virtual ~Texture();

    virtual bool Load(bool reload = false);

    void Activate(GLenum textureNumber) const;
    void Bind() const;
    void Unbind() const;

    static Texture &DefaultTexture();

    unsigned int id;
    const char *filepath;
    bool loaded;
    glm::ivec2 size;
    glm::vec2 scale;
};

#endif
