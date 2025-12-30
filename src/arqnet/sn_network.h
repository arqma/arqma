#pragma once

#include <zmq.hpp>
#include "bt_serialize.h"
#include <string>
#include <list>
#include <unordered_map>
#include <memory>
#include <functional>
#include <thread>
#include <mutex>
#include <iostream>
#include <chrono>

namespace arqnet {

/// Logging levels passed into LogFunc
enum class LogLevel { trace, debug, info, warn, error, fatal };

using namespace std::chrono_literals;

class SNNetwork {
public:
    /// The function type for looking up where to connect to the SN with the given pubkey.  Should
    /// return an empty string for an invalid or unknown pubkey or one without a known address.
    using LookupFunc = std::function<std::string(const std::string &pubkey)>;

    enum class allow { denied, client, service_node };

    /// Callback type invoked to determine whether the given ip and pubkey are allowed to connect to
    /// us.  This will be called in two contexts:
    ///
    /// - if the connection is from another SN for quorum-related duties, the pubkey will be set
    ///   to the verified pubkey of the remote service node.
    /// - otherwise, for a client connection (for example, a regular node connecting to submit a
    ///   blink transaction to a blink quorum) the pubkey will be empty.
    ///
    /// @param ip - the ip address of the incoming connection
    /// @param pubkey - the curve25519 pubkey (which is calculated from the SN ed25519 pubkey) of
    /// the connecting service node (32 byte string), or an empty string if this is a client
    /// connection without remote SN authentication.
    using AllowFunc = std::function<allow(const std::string &ip, const std::string &pubkey)>;

    /// Function pointer to ask whether a log of the given level is wanted.  If it returns true the
    /// log message will be built and then passed to Log.
    using WantLog = bool (*)(LogLevel);
    ///
    /// call to get somewhere to log to when there is a logging message.  If it
    /// returns a std::ostream pointer then output is sent to it; otherwise (i.e. nullptr) output is
    /// suppressed.  Takes three arguments: the log level, the __FILE__ value, and the __LINE__ value.
    using WriteLog = void (*)(LogLevel level, const char *file, int line, std::string msg);

    /// Explicitly non-copyable, non-movable because most things here aren't copyable, and a few
    /// things aren't movable.  If you need to pass the SNNetwork around, wrap it in a unique_ptr.
    SNNetwork(const SNNetwork &) = delete;
    SNNetwork &operator=(const SNNetwork &) = delete;
    SNNetwork(SNNetwork &&) = delete;
    SNNetwork &operator=(SNNetwork &&) = delete;

    /// Encapsulates an incoming message from a remote node with message details plus extra info
    /// need to send a reply back through the proxy thread via the `reply()` method.
    class message {
    private:
        SNNetwork &net;
    public:
        std::string command; ///< The command name
        std::vector<bt_value> data; ///< The provided command data parts, if any.
        const std::string pubkey; ///< The originator pubkey (32 bytes)
        const bool sn;

        /// Constructor
        message(SNNetwork &net, std::string command, std::string pubkey, bool sn)
            : net{net}, command{std::move(command)}, pubkey{std::move(pubkey)}, sn{sn} {
            assert(this->pubkey.size() == 32);
        }

        template <typename... Args>
        void reply(const std::string &command, Args &&...args);
    };

    /// Opaque pointer sent to the callbacks, to allow for including arbitrary state data (for
    /// example, an owning object or state data).  Defaults to nullptr and has to be explicitly set
    /// if desired.
    void *data = nullptr;

    enum class command_type { quorum, public_, response };

private:
    zmq::context_t context;

    /// A unique id for this SNNetwork, assigned in a thread-safe manner during construction.
    const int object_id;

    std::string pubkey, privkey;

    /// The thread in which most of the intermediate work happens (handling external connections
    /// and proxying requests between them to worker threads)
    std::thread proxy_thread;

    /// Called to obtain a "command" socket that attaches to `control` to send commands to the
    /// proxy thread from other threads.  This socket is unique per thread and SNNetwork instance.
    zmq::socket_t &get_control_socket();
    /// Stores all of the sockets created in different threads via `get_control_socket`.  This is
    /// only used during destruction to close all of those open sockets, and is protected by an
    /// internal mutex which is only locked by new threads getting a control socket and the
    /// destructor.
    std::vector<std::shared_ptr<zmq::socket_t>> thread_control_sockets;




    ///////////////////////////////////////////////////////////////////////////////////
    /// NB: The following are all the domain of the proxy thread (once it is started)!

    /// The lookup function that tells us where to connect to a peer
    LookupFunc peer_lookup;

    std::unique_ptr<zmq::socket_t> listener;

    /// Callback to see whether the incoming connection is allowed
    AllowFunc allow_connection;

    /// Callback to see if we want a log message at the given level.
    WantLog want_logs;
    /// If want_logs returns true, the log message is build and passed into this.
    WriteLog logger;

    /// Info about a peer's established connection to us.  Note that "established" means both
    /// connected and authenticated.
    struct peer_info {
      bool service_node = false;
      std::string incoming;
      int outgoing = -1;
      std::chrono::steady_clock::time_point last_activity;
      void activity() { last_activity = std::chrono::steady_clock::now(); }
      std::chrono::milliseconds idle_expiry;
    };

    struct pk_hash { size_t operator()(const std::string &pubkey) const { size_t h; std::memcpy(&h, pubkey.data(), sizeof(h)); return h; } };

    /// Currently peer connections, pubkey -> peer_info
    std::unordered_map<std::string, peer_info, pk_hash> peers;

    /// different polling sockets the proxy handler polls: this always contains some internal
    /// sockets for inter-thread communication followed by listener socket and a pollitem for every
    /// (outgoing) remote socket in `remotes`.  This must be in a sequential vector because of zmq
    /// requirements (otherwise it would be far nicer to not have to synchronize these two vectors).
    std::vector<zmq::pollitem_t> pollitems;

    /// Properly adds a socket to poll for input to pollitems
    void add_pollitem(zmq::socket_t &sock);

    /// The number of internal sockets in `pollitems`
    static constexpr size_t poll_internal_size = 3;

    /// The pollitems location corresponding to `remotes[0]`.
    const size_t poll_remote_offset;

    /// The outgoing remote connections we currently have open.  Note that they are generally
    /// accessed via the weak_ptr inside the `peers` element.  Each element [i] here corresponds to
    /// an the pollitem_t at pollitems[i+1+poll_internal_size].  (Ideally we'd use one structure,
    /// but zmq requires the pollitems be in contiguous storage).
    std::vector<std::pair<std::string, zmq::socket_t>> remotes;

    /// Socket we listen on to receive control messages in the proxy thread. Each thread has its own
    /// internal "control" connection (returned by `get_control_socket()`) to this socket used to
    /// give instructions to the proxy such as instructing it to initiate a connection to a remote
    /// or send a message.
    zmq::socket_t command{context, zmq::socket_type::router};

    /// Router socket to reach internal worker threads from proxy
    zmq::socket_t workers{context, zmq::socket_type::router};

    /// Starts a new worker thread with the given id.  Note that the worker may not yet be ready
    /// until a READY signal arrives on the worker socket.
    void spawn_worker(std::string worker_id);

    /// Worker threads (ZMQ id -> thread)
    std::unordered_map<std::string, std::thread> worker_threads;

    /// ZMQ ids of idle, active workers
    std::list<std::string> idle_workers;

    /// Maximum number of worker threads created on demand up to this limit.
    unsigned int max_workers;

    /// Worker thread loop
    void worker_thread(std::string worker_id);

    /// Does the proxying work
    void proxy_loop(const std::vector<std::string> &bind);

    void proxy_to_worker(size_t conn_index, std::list<zmq::message_t> &&parts);

    /// proxy thread command handlers for commands sent from the outer object QUIT.  This doesn't
    /// get called immediately on a QUIT command: the QUIT commands tells workers to quit, then this
    /// gets called after all works have done so.
    void proxy_quit();

    std::pair<zmq::socket_t *, std::string> proxy_connect(const std::string &pubkey, const std::string &connect_hint, bool optional, bool incoming_only, std::chrono::milliseconds keep_alive);

    /// CONNECT command telling us to connect to a new pubkey.  Returns the socket (which could be
    /// existing or a new one).
    std::pair<zmq::socket_t *, std::string> proxy_connect(bt_dict &&data);

    void proxy_disconnect(const std::string &pubkey);

    /// SEND command.  Does a connect first, if necessary.
    void proxy_send(bt_dict &&data);

    /// REPLY command.  Like SEND, but only has a listening socket route to send back to and so is
    /// weaker (i.e. it cannot reconnect to the SN if the connection is no longer open).
    void proxy_reply(bt_dict &&data);

    /// ZAP (https://rfc.zeromq.org/spec:27/ZAP/) authentication handler; this is called with the
    /// zap auth socket to do non-blocking processing of any waiting authentication requests waiting
    /// on it to verify whether the connection is from a valid/allowed SN.
    void process_zap_requests(zmq::socket_t &zap_auth);

    /// Handles a control message from some outer thread to the proxy
    void proxy_control_message(std::list<zmq::message_t> parts);

    /// Closing any idle connections that have outlived their idle time.  Note that this only
    /// affects outgoing connections; incomings connections are the responsibility of the other end.
    void proxy_expire_idle_peers();

    decltype(peers)::iterator proxy_close_outgoing(decltype(peers)::iterator it);


    /// End of proxy-specific members
    ///////////////////////////////////////////////////////////////////////////////////




    /// Callbacks for data commands.  Must be fully populated before starting SNNetwork instances
    /// as this is accessed without a lock from worker threads.
    ///
    /// The value is the {callback, public} pair where `public` is true if unauthenticated
    /// connections may call this and false if only authenricated SNs may invoke the command.
    static std::unordered_map<std::string, std::pair<void(*)(SNNetwork::message &message, void *data), command_type>> commands;
    static bool commands_mutable;

    void launch_proxy_thread(const std::vector<std::string> &bind);

public:
    /**
     * Constructs a SNNetwork connection listening on the given bind string.
     *
     * @param pubkey the service node's public key (32-byte binary string)
     * @param privkey the service node's private key (32-byte binary string)
     * @param bind list of addresses to bind to.  Can be any string zmq supports; typically a tcp
     * IP/port combination such as: "tcp://\*:4567" or "tcp://1.2.3.4:5678".
     * @param peer_lookup function that takes a pubkey key (32-byte binary string) and returns a
     * connection string such as "tcp://1.2.3.4:23456" to which a connection should be established
     * to reach that service node.  Note that this function is only called if there is no existing
     * connection to that service node, and that the function is never called for a connection to
     * self (that uses an internal connection instead).
     * @param allow_incoming called on incoming connections with the (verified) incoming connection
     * pubkey (32-byte binary string) to determine whether the given SN should be allowed to
     * connect.
     * @param want_log
     * @param log a function pointer (or non-capturing lambda) to call to get a std::ostream pointer
     * to send output to, or nullptr to suppress output.  Optional; if omitted the default returns
     * std::cerr for WARN and higher.
     * @param max_workers the maximum number of simultaneous worker threads to start.  Defaults to
     * std::thread::hardware_concurrency().  Note that threads are only started on demand (i.e. when
     * a request arrives when all existing threads are busy handling requests).
     */
    SNNetwork(std::string pubkey, std::string privkey,
            const std::vector<std::string> &bind,
            LookupFunc peer_lookup,
            AllowFunc allow_connection,
            WantLog want_log = [](LogLevel l) { return l >= LogLevel::warn; },
            WriteLog logger = [](LogLevel, const char *f, int line, std::string msg) { std::cerr << f << ':' << line << ": " << msg << std::endl; },
            unsigned int max_workers = std::thread::hardware_concurrency());

    /** Constructs a SNNetwork that does not listen but can be used for connecting to remote
     * listening service nodes, for example to submit blink transactions to service nodes.  It
     * generates a non-persistant x25519 key pair on startup (for encrypted communication with
     * peers).
     */
    SNNetwork(LookupFunc peer_lookup,
            AllowFunc allow_connection,
            WantLog want_log = [](LogLevel l) { return l >= LogLevel::warn; },
            WriteLog logger = [](LogLevel, const char *f, int line, std::string msg) { std::cerr << f << ':' << line << ": " << msg << std::endl; },
            unsigned int max_workers = std::thread::hardware_concurrency());

    /**
     * Destructor; instructs the proxy to quit.  The proxy tells all workers to quit, waits for them
     * to quit and rejoins the threads then quits itself.  The outer thread (where the destructor is
     * running) rejoins the proxy thread.
     */
    ~SNNetwork();

    bool is_service_node() const { return (bool) listener; }

    /**
     * Try to initiate a connection to the given SN in anticipation of needing a connection in the
     * future.  If a connection is already established, the connection's idle timer will be reset
     * (so that the connection will not be closed too soon).  If the given idle timeout is greater
     * than the current idle timeout then the timeout increases to the new value; if less than the
     * current timeout it is ignored.
     *
     * Note that this method (along with send) doesn't block waiting for a connection; it merely
     * instructs the proxy thread that it should establish a connection.
     *
     * @param pubkey - the public key (32-byte binary string) of the service node to connect to
     * @param idle_timeout - the connection will be kept alive if there was valid activity within
     *                       the past `keep_alive` milliseconds.  If a connection already exists,
     *                       the longer of the existing and the given keep alive is used.
     */
    void connect(const std::string &pubkey, std::chrono::milliseconds keep_alive = 5min, const std::string &hint = "");

    /**
     * Queue a message to be relayed to SN identified with the given pubkey without expecting a
     * reply.  The SN will attempt to relay (first connecting and handshaking if not already
     * connected to the given SN).
     *
     * If a new connection it established it will have a relatively short (15s) idle timeout.  If
     * the connection should stay open longer you should call `connect(pubkey, IDLETIME)` first.
     *
     * Note that this method (along with connect) doesn't block waiting for a connection or for the
     * message to send; it merely instructs the proxy thread that it should send.
     *
     * @param pubkey - the pubkey to send this to
     * @param value - a bt_serializable value to serialize and send
     * @param opts - any number of bt_serializable values and send options.  Each send option affect
     *               how the send works; each serializable value becomes a serialized message part.
     *
     * Example:
     *
     *     sn.send(pubkey, "hello", "abc", 42, send_option::hint("tcp://localhost:1234"));
     *
     * sends the command `hello` to the given pubkey, containing two additional message parts of
     * serialized "abc" and 42 values, and, if not currently connected, using the given connection
     * hint rather than performing a connection address lookup on the pubkey.
     */
    template <typename... T>
    void send(const std::string &pubkey, const std::string &cmd, const T &...opts);

    static constexpr std::chrono::milliseconds default_send_keep_alive{15000};

    /// The key pair this SN was created with
    const std::string &get_pubkey() const { return pubkey; }
    const std::string &get_privkey() const { return privkey; }

    static void register_command(std::string command, command_type cmd_type, void(*callback)(SNNetwork::message &message, void *data));
};

namespace send_option {

struct serialized {
  std::string data;
  template <typename T>
  serialized(const T &arg) : data{arqnet::bt_serialize(arg)} {}
};

struct hint {
  std::string connect_hint;
  hint(std::string connect_hint) : connect_hint{std::move(connect_hint)} {}
};

struct optional {};

struct incoming {};

struct keep_alive {
  std::chrono::milliseconds time;
  keep_alive(std::chrono::milliseconds time) : time{std::move(time)} {}
};

}

namespace detail {

void send_control(zmq::socket_t &sock, const std::string &cmd, const bt_dict &data = {});

template <typename T>
void apply_send_option(bt_list &parts, bt_dict &, const T &arg)
{
  parts.push_back(arqnet::bt_serialize(arg));
}

template <> inline void apply_send_option(bt_list &parts, bt_dict &, const send_option::serialized &serialized)
{
  parts.push_back(serialized.data);
}

template <> inline void apply_send_option(bt_list &, bt_dict &control_data, const send_option::hint &hint)
{
  control_data["hint"] = hint.connect_hint;
}

template <> inline void apply_send_option(bt_list &, bt_dict &control_data, const send_option::optional &)
{
  control_data["optional"] = 1;
}

template <> inline void apply_send_option(bt_list &, bt_dict &control_data, const send_option::incoming &)
{
  control_data["incoming"] = 1;
}

template <> inline void apply_send_option(bt_list &, bt_dict &control_data, const send_option::keep_alive &timeout)
{
  control_data["keep-alive"] = timeout.time.count();
}

template <typename... T>
bt_dict send_control_data(const std::string &cmd, const T &...opts)
{
  bt_dict control_data;
  bt_list parts{{cmd}};

#ifdef __cpp_fold_expressions
  (detail::apply_send_option(parts, control_data, opts),...);
#else
  (void) std::initializer_list<int>{(detail::apply_send_option(parts, control_data, opts), 0)...};
#endif

  control_data["send"] = std::move(parts);
  return control_data;
}

}

template <typename... T>
void SNNetwork::send(const std::string &pubkey, const std::string &cmd, const T &...opts)
{
  bt_dict control_data = detail::send_control_data(cmd, opts...);
  control_data["pubkey"] = pubkey;
  detail::send_control(get_control_socket(), "SEND", control_data);
}

template <typename... Args>
void SNNetwork::message::reply(const std::string &command, Args &&...args)
{
  if (sn) net.send(pubkey, command, std::forward<Args>(args)...);
  else net.send(pubkey, command, send_option::optional{}, std::forward<Args>(args)...);
}

// Creates a hex string from a character sequence.
template <typename It>
std::string as_hex(It begin, It end) {
    constexpr std::array<char, 16> lut{{'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'}};
    std::string hex;
    using std::distance;
    hex.reserve(distance(begin, end) * 2);
    while (begin != end) {
        char c = *begin;
        hex += lut[(c & 0xf0) >> 4];
        hex += lut[c & 0x0f];
        ++begin;
    }
    return hex;
}

template <typename String>
inline std::string as_hex(const String &s)
{
  using std::begin;
  using std::end;
  return as_hex(begin(s), end(s));
}



}
