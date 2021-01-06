#ifndef AFINA_NETWORK_ST_NONBLOCKING_CONNECTION_H
#define AFINA_NETWORK_ST_NONBLOCKING_CONNECTION_H

#include <afina/execute/Command.h>
#include <cstring>
#include <protocol/Parser.h>
#include <spdlog/logger.h>
#include <sys/epoll.h>
#include <vector>

namespace Afina {
namespace Network {
namespace STnonblock {

class Connection {
public:
    Connection(int s, std::shared_ptr<Afina::Storage> &ps, std::shared_ptr<spdlog::logger> &pl)
        : _is_alive(true),  
          _end_reading(false),
          _socket(s),
          _max_output_queue_size(4096),  
          _read_bytes(0),
          _head_written_count(0),
          _pStorage(ps),
          _logger(pl),
          _arg_remains(0) {

        std::memset(&_event, 0, sizeof(struct epoll_event));
        _event.data.ptr = this;
        std::memset(_read_buffer, 0, 4096);
    }

    inline bool isAlive() const { return _is_alive; }

    void Start();

protected:
    void OnError();
    void OnClose();
    void Close();
    void DoRead();
    void DoWrite();

private:
    friend class ServerImpl;

    bool _is_alive;
    bool _end_reading;
    int _socket;
    struct epoll_event _event;

    std::vector<std::string> _output_queue;
    size_t _max_output_queue_size;
    char _read_buffer[4096];
    size_t _read_bytes;
    int _head_written_count;
    std::shared_ptr<Afina::Storage> _pStorage;
    std::shared_ptr<spdlog::logger> _logger;

    std::size_t _arg_remains;
    Protocol::Parser _parser;
    std::string _argument_for_command;
    std::unique_ptr<Execute::Command> _command_to_execute;
};

} // namespace STnonblock
} // namespace Network
} // namespace Afina

#endif // AFINA_NETWORK_ST_NONBLOCKING_CONNECTION_H