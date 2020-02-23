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

    zmq::message_t ArqmaNotifier::create_message(std::string &&data) {
        auto *buffer = new std::string(std::move(data));
        return zmq::message_t{&(*buffer)[0], buffer->size(), message_buffer_destroy, buffer};
    };

    void ArqmaNotifier::notify(std::string &&data) {
        std::cout << data << std::endl;
        //auto *buffer = new std::string(std::move(data));
        //zmq::message_t msg = zmq::message_t{&(*buffer)[0], buffer->size(), message_buffer_destroy, buffer};
//        std::string id = new std::string("getblocktemplate");
        socket.send(create_message(std::move("getblocktemplate")), ZMQ_SNDMORE);
        socket.send(create_message(std::move(data)), 0);
    }
}
