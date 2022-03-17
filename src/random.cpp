#include <random>

#include "random.h"

float RandomFloat()
{
    static std::random_device rd;
    static std::default_random_engine eng(rd());
    static std::uniform_real_distribution<> distr(0.f, 1.f);
    return distr(eng);
}
