#include "rand.h"

// The Linear Congruential Generator
// Xn = (a * Xn-1 + b) % m.
// Each of these members have to satisfy the following conditions:
// m > 0 (the modulus is positive),
// 0 < a < m (the multiplier is positive but less than the modulus),
// 0 ≤ b < m (the increment is non negative but less than the modulus), and
// 0 ≤ X0 < m (the seed is non negative but less than the modulus).

// TODO: This rand module is only for testing kernel, need to be improved.
static uint32_t x = 0;
static uint32_t a = 1234;
static uint32_t b = 5678;
static uint32_t m = 20170;

void rand_set_seed(uint32_t seed) { x = seed; }

uint32_t rand(void) {
  x = (a * x + b) % m;
  return x;
}
