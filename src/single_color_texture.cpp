#include "single_color_texture.h"

SingleColorTexture::SingleColorTexture(unsigned char r, unsigned char g, unsigned char b)
    : Texture(glm::ivec2(1, 1)), rgb({r, g, b})
{
    Load();
}

SingleColorTexture::~SingleColorTexture()
{
}

bool SingleColorTexture::Load(bool _)
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb);
    glGenerateMipmap(GL_TEXTURE_2D);

    loaded = true;
    return true;
}

/*static*/ Texture &SingleColorTexture::DefaultTexture()
{
    static SingleColorTexture texture(255, 0, 255);

    return texture;
}
