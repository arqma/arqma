#include "arqmaMQ.h"

namespace arqmaMQ 
{
    extern "C" void message_buffer_destroy(void*, void* hint) {
        delete reinterpret_cast<std::string*>(hint);
    }

    ArqmaNotifier::ArqmaNotifier()
    {
        socket.bind("tcp://127.0.0.1:3000");
    }

    ArqmaNotifier::~ArqmaNotifier()
    {
        zmq_close(&socket);
    }

    void ArqmaNotifier::notify(std::string &&data) {
        std::cout << data << std::endl;
        auto *buffer = new std::string(std::move(data));
        zmq::message_t msg = zmq::message_t{&(*buffer)[0], buffer->size(), message_buffer_destroy, buffer};
        socket.send(msg, ZMQ_NOBLOCK);
    }
}
