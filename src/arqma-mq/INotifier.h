#pragma once

#include <string>

namespace arqmaMQ {

    class INotifier {
        public:
            virtual void notify(std::string &&data) = 0;
    };
}

