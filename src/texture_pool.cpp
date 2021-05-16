#include "texture_pool.h"

#include <unordered_map>

static std::unordered_map<std::string, Texture> textures;

Texture *GetTexture(std::string texture_path)
{
    if (textures.find(texture_path) == textures.end())
        textures[texture_path] = Texture(texture_path.c_str(), true);

    return &textures[texture_path];
}
