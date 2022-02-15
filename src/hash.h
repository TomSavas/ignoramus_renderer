#pragma once

inline unsigned long Djb2Byte(char c, unsigned long hash = 5381)
{
    return ((hash << 5) + hash) + c;   
}

inline unsigned long Djb2(const unsigned char* str, unsigned long hash = 5381)
{
    for (char c = *str; c != '\0'; c = *(++str))
    {
        hash = Djb2Byte(c, hash);
    }

    return hash;
}
