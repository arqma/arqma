#pragma once

#include <cstdint>

namespace cryptonote
{
  enum class txversion : uint16_t {
    v0 = 0,
    v1,
    v2,
    v3,

    _count
  };

  enum class txtype : uint16_t {
    standard = 0,
    state_change,
    key_image_unlock,
    stake,

    _count
  };

}
