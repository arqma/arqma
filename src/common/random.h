#pragma once

#include <random>

namespace tools {

extern thread_local std::mt19937_64 rng;

uint64_t uniform_distribution_portable(std::mt19937_64& rng, uint64_t n);

template <typename RandomIt>
void shuffle_portable(RandomIt begin, RandomIt end, uint64_t seed)
{
  if (end <= begin + 1) return;
  const size_t size = std::distance(begin, end);
  std::mt19937_64 rng{seed};
  for (size_t i = 1; i < size; i++)
  {
    size_t j = (size_t)uniform_distribution_portable(rng, i+1);
    using std::swap;
    swap(begin[i], begin[j]);
  }
}

};
