#pragma once

#include "crypto/hash.h"
#include "cryptonote_basic/cryptonote_basic.h"

namespace cryptonote
{
  struct txpool_event
  {
    cryptonote::transaction tx;
    crypto::hash hash;
    bool res;
  };
}
