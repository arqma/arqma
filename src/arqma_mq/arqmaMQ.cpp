#include "arqmaMQ.h"

namespace arqmaMQ 
{
    ArqmaNotifier::ArqmaNotifier()
    {
//        zmq::socket_t socket(context, ZMQ_PUB);
//        socket.bind("tcp://127.0.0.1:3000");
    }

    ArqmaNotifier::~ArqmaNotifier(){}

    void ArqmaNotifier::notify(std::string &&data) {
        //auto *buffer = new std::string(std::move(data));
        std::cout << data << std::endl;
//        socket.send(zmq::buffer(data), zmq::send_flags::dontwait);
    }
}
