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
    {}

    ArqmaNotifier::~ArqmaNotifier()
    {
        producer.send(create_message(std::move("QUIT")), 0);
        proxy_thread.join();
        zmq_close(&producer);
        zmq_close(&subscriber);
        zmq_term(&context);
    }

    void ArqmaNotifier::run()
    {
        producer.bind("inproc://backend");
        proxy_thread = std::thread{&ArqmaNotifier::proxy_loop, this};
    }

    bool ArqmaNotifier::addTCPSocket(boost::string_ref address, boost::string_ref port)
    {
/*
        if(!context)
        {
            MERROR("ZMQ Server is already shutdowned");
            return false;
        }
*/
/*
        rep_socket.reset(zmq_socket(context.get(), ZMQ_REP));
        if(!rep_socket)
        {
            ARQMA_LOG_ZMQ_ERROR("ZMQ Server socket create failed");
            return false;
        }

        if(zmq_setsockopt(rep_socket.get(), ZMQ_MAXMSGSIZE, std::addressof(max_message_size), sizeof(max_message_size)) != 0)
        {
            ARQMA_LOG_ZMQ_ERROR("Failed to set maximum incoming message size");
            return false;
        }
*/
/*
        static constexpr const int linger_value = std::chrono::milliseconds{linger_timeout}.count();
        if (zmq_setsockopt(rep_socket.get(), ZMQ_LINGER, std::addressof(linger_value), sizeof(linger_value)) != 0)
        {
            ARQMA_LOG_ZMQ_ERROR("Failed to set linger timeout");
            return false;
        }
*/

        if(address.empty())
            address = "*";
        if(port.empty())
            port = "*";

        bind_address.append(address.data(), address.size());
        bind_address += ":";
        bind_address.append(port.data(), port.size());

/*
        if(zmq_bind(rep_socket.get(), bind_address.c_str()) < 0)
        {
            ARQMA_LOG_ZMQ_ERROR("ZMQ Server bind failed");
            return false;
        }
*/
        return true;
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
        listener.bind(bind_address);

        zmq::pollitem_t items[2];
        items[0].socket = (void*)subscriber;
        items[0].fd = 0;
        items[0].events = ZMQ_POLLIN;
        items[1].socket = (void*)listener;
        items[1].fd = 0;
        items[1].events = ZMQ_POLLIN;


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
                    for(auto &remote : remotes)
                    {
		    			std::string response = handler.handle(remote.second);
                    	LOG_PRINT_L1("sending client " << remote.first << " " << response);
                    	s_sendmore(listener, remote.first);
                	    s_send(listener, response);
                    }
				}
				else 
				{
               		listener.recv(&envelope1);
               		listener.recv(&envelope1);
               		std::string request = std::string(static_cast<char*>(envelope1.data()), envelope1.size());
                	LOG_PRINT_L1("received from client " <<  request);
                    if (request.compare(EVICT) == 0)
                    {
                        remotes.erase(remote_identifier);
                    }
                    else
                    {
                        std::string response = handler.handle(request);
                        LOG_PRINT_L1("sending client " << remote_identifier << " " << response);
                        s_sendmore(listener, remote_identifier);
                        s_send(listener, response);
            	        remotes.insert(std::pair<std::string, std::string>(std::move(remote_identifier), std::move(request)));
                    }
				}
            }



            if (items[0].revents & ZMQ_POLLIN)
            {

                zmq::message_t envelope;
                subscriber.recv(&envelope);
                std::string stop = std::string(static_cast<char*>(envelope.data()), envelope.size());
                if (stop == QUIT) 
                {
                    LOG_PRINT_L1("closing thread");
                    break;
                }
            }
        }
        zmq_close(&listener);
    }
}
