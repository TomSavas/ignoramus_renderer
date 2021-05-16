#include "texture.h"

class SingleColorTexture 
    : public Texture
{
public:
    SingleColorTexture(unsigned char r, unsigned char g, unsigned char b);
    virtual ~SingleColorTexture();

    virtual bool Load(bool _ = false) override;

    static Texture &DefaultTexture();

protected:
    unsigned char rgb[3];
};
