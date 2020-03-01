#pragma once

#include <string>

#include "cryptonote_basic/cryptonote_basic_impl.h"

using namespace cryptonote;


namespace arqmaMQ {

    class INotifier {
        public:
            virtual void notify(std::string &&data) = 0;
    };
}

