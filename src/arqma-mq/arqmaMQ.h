#pragma once

#include <string>


namespace arqmaMQ {

    class INotifier {
        public:
            virtual void notify(std::string &&data) = 0;
    };


    class ArqmaNotifier: public INotifier {
        public:
            void notify(std::string &&data) {
                //auto *buffer = new std::string(std::move(data));
                std::cout << data << std::endl;
            }
    };
}
