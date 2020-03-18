#pragma once

#include <string>
#include <boost/utility/string_ref.hpp>
#include "cryptonote_basic/cryptonote_basic_impl.h"

using namespace cryptonote;


namespace arqmaMQ {

    class INotifier {
        public:
            virtual bool addTCPSocket(boost::string_ref address, boost::string_ref port, uint16_t maxclients) = 0;
            virtual void run() = 0;
            virtual void stop() = 0;
    };
}

