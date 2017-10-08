#ifndef RANDOM_HELPER_H
#define RANDOM_HELPER_H

#include <random>

extern std::mt19937_64 globalGenerator;

void seedGenerator();

#endif /* RANDOM_HELPER_H */
