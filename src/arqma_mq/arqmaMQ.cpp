#include "arqmaMQ.h"


namespace arqmaMQ 
{
    extern "C" void message_buffer_destroy(void*, void* hint) {
        delete reinterpret_cast<std::string*>(hint);
    }

    inline static int
    s_send(void *socket, const char *string, int flags = 0) {
        int rc;
        zmq_msg_t message;
        zmq_msg_init_size(&message, strlen(string));
        memcpy(zmq_msg_data(&message), string, strlen(string));
        rc = zmq_msg_send(&message, socket, flags);
        assert(-1 != rc);
        zmq_msg_close(&message);
        return (rc);
    }

    inline static bool
    s_send (zmq::socket_t & socket, const std::string & string, int flags = 0) {

        zmq::message_t message(string.size());
        memcpy (message.data(), string.data(), string.size());
        bool rc = socket.send (message, flags);
        return (rc);
    }

    inline static bool 
    s_sendmore (zmq::socket_t & socket, const std::string & string) {
        zmq::message_t message(string.size());
        memcpy (message.data(), string.data(), string.size());
        bool rc = socket.send (message, ZMQ_SNDMORE);
        return (rc);
    }


    ArqmaNotifier::ArqmaNotifier(DaemonHandler& h): handler(h)
    {
        producer.bind("inproc://backend");
        proxy_thread = std::thread{&ArqmaNotifier::proxy_loop, this};
    }

    ArqmaNotifier::~ArqmaNotifier()
    {
        producer.send(create_message(std::move("QUIT")), 0);
        proxy_thread.join();
        zmq_close(&producer);
        zmq_close(&subscriber);
        zmq_term(&context);
    }

    zmq::message_t ArqmaNotifier::create_message(std::string &&data)
    {
        auto *buffer = new std::string(std::move(data));
        return zmq::message_t{&(*buffer)[0], buffer->size(), message_buffer_destroy, buffer};
    };

    void ArqmaNotifier::notify(std::string &&data)
    {
        //std::cout << data << std::endl;
        //producer.send(create_message(std::move("getblocktemplate")), ZMQ_SNDMORE);
        //producer.send(create_message(std::move(data)), 0);
    }

    void ArqmaNotifier::proxy_loop()
    {
        subscriber.connect("inproc://backend");
        listener.bind("tcp://*:3000");

        zmq::pollitem_t items[2];
        items[0].socket = (void*)subscriber;
        items[0].fd = 0;
        items[0].events = ZMQ_POLLIN;
        items[1].socket = (void*)listener;
        items[1].fd = 0;
        items[1].events = ZMQ_POLLIN;

        std::string quit("QUIT");

        while (true) 
        {
			std::string  block_hash;
            int rc = zmq::poll(items, 2, 1);

            if (items[1].revents & ZMQ_POLLIN)
   	        {
       	        zmq::message_t envelope1;
           	    listener.recv(&envelope1);
           	 	std::string remote_identifier = std::string(static_cast<char*>(envelope1.data()), envelope1.size());
           	 	//TODO: record <id, command>
				if (remote_identifier.compare("block") == 0)
				{
               		listener.recv(&envelope1);
               		listener.recv(&envelope1);
               		block_hash = std::string(static_cast<char*>(envelope1.data()), envelope1.size());
                	LOG_PRINT_L1("received from blockchain " <<  block_hash);
				}
				else 
				{
               		listener.recv(&envelope1);
               		listener.recv(&envelope1);
               		std::string request = std::string(static_cast<char*>(envelope1.data()), envelope1.size());
                	LOG_PRINT_L1("received from client " <<  request);
            	    remotes.insert(std::pair<std::string, std::string>(std::move(remote_identifier), std::move(request)));
				}
				//TODO: iterate list of <id, command>
                for(auto &remote : remotes)
                {
					std::string response = handler.handle(remote.second); //handler.handle("{\"jsonrpc\": \"2.0\", \"id\" : \"1\", \"method\" : \"get_info\", \"params\": {}}");
                	LOG_PRINT_L1("sending client " << remote.first << " " << response);
                	s_sendmore(listener, remote.first);
                	s_send(listener, response);
                }
            }



            if (items[0].revents & ZMQ_POLLIN)
            {

                zmq::message_t envelope;
                subscriber.recv(&envelope);
                std::string stop = std::string(static_cast<char*>(envelope.data()), envelope.size());
                if (stop == quit) 
                {
                    LOG_PRINT_L1("closing thread");
                    break;
                }
            }
        }
        zmq_close(&listener);
    }
}
