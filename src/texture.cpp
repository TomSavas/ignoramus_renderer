#include <stdio.h>

#include "texture.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

Texture::Texture(glm::ivec2 size, glm::vec2 scale)
    : loaded(false), filepath(""), size(size), scale(scale)
{
    glGenTextures(1, &id);
}

Texture::Texture(const char *filepath, bool load, glm::vec2 scale)
    : Texture(glm::ivec2(), scale)
{
    this->filepath = filepath;

    if (load)
    {
        Load();
        //Activate(GL_TEXTURE0);
    }
}

// temp
Texture::Texture(unsigned int id, glm::ivec2 size, glm::vec2 scale)
    : id(id), size(size), scale(scale), loaded(true)
{
}

Texture::~Texture()
{
}

bool Texture::Load(bool reload)
{
    if (loaded && !reload)
        return true;

    glBindTexture(GL_TEXTURE_2D, id);

    if (filepath != "")
    {
        int _;

        stbi_set_flip_vertically_on_load(true);
        int componentNum;
        unsigned char *texture = stbi_load(filepath, &size.x, &size.y, &componentNum, 0);

        if (texture == nullptr)
        {
            printf("stb failed loading %s\n", filepath);
            return false;
        }
        printf("loading texture %s %dx%d\n", filepath, size.x, size.y);

        GLenum format;
        switch(componentNum)
        {
        case 1:
            format = GL_RED;
            break;
        case 3:
            format = GL_RGB;
            break;
        case 4:
            format = GL_RGBA;
            break;
        }

        glTexImage2D(GL_TEXTURE_2D, 0, format, size.x, size.y, 0, format, GL_UNSIGNED_BYTE, texture);
        glGenerateMipmap(GL_TEXTURE_2D);

        //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(texture);
    }
    else if (!reload)
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, size.x, size.y, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    loaded = true;

    return true;
}

void Texture::Activate(GLenum textureNumber) const
{
    assert(("Cannot bind an unloaded texture", loaded));

    glActiveTexture(textureNumber);
    Bind();
}

void Texture::Bind() const
{
    assert(("Cannot bind an unloaded texture", loaded));

    glBindTexture(GL_TEXTURE_2D, id);
}

void Texture::Unbind() const
{
    assert(("Cannot unbind an unloaded texture", loaded));

    glBindTexture(GL_TEXTURE_2D, 0);
}

/* static */ Texture &Texture::DefaultTexture()
{
    static Texture texture ("../assets/textures/default.jpeg", true);

    return texture;
}
