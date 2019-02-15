#include <random>
#include "random_helper.h"

std::mt19937_64 globalGenerator;

void seedGenerator()
{
    globalGenerator.seed(SEED);
}
