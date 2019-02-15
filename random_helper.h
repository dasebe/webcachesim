#ifndef RANDOM_HELPER_H
#define RANDOM_HELPER_H

#include <random>

const unsigned int SEED = 1534262824; // const seed for repeatable results
extern std::mt19937_64 globalGenerator;

void seedGenerator();

#endif /* RANDOM_HELPER_H */
