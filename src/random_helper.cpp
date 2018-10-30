#include <random>
#include "random_helper.h"

std::mt19937_64 globalGenerator;

void seedGenerator()
{
    //TODO implement better seeding
    std::random_device rd;
    globalGenerator.seed(rd());
}
