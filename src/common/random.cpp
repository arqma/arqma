#include "random.h"
#include <cassert>

namespace tools {

thread_local std::mt19937_64 rng{std::random_device{}()};

uint64_t uniform_distribution_portable(std::mt19937_64& rng, const uint64_t n)
{
  assert(n > 0);
  const uint64_t secureMax = rng.max() - rng.max() % n;
  uint64_t x;
  do x = rng(); while (x >= secureMax);
  return x / (secureMax / n);
}

}
