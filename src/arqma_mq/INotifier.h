#pragma once

#include <string>
#include <boost/utility/string_ref.hpp>
#include "cryptonote_basic/cryptonote_basic_impl.h"

using namespace cryptonote;


namespace arqmaMQ {

    class INotifier {
        public:
            virtual void notify(std::string &&data) = 0;
            virtual bool addTCPSocket(boost::string_ref address, boost::string_ref port) = 0;
            virtual void run() = 0;
    };
}

