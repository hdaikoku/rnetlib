#ifndef RNETLIB_RNETLIB_H_
#define RNETLIB_RNETLIB_H_

#include "rnetlib/client.h"
#include "rnetlib/event_loop.h"
#include "rnetlib/server.h"
#include "rnetlib/socket/socket_client.h"
#include "rnetlib/socket/socket_event_loop.h"
#include "rnetlib/socket/socket_server.h"

#ifdef RNETLIB_USE_RDMA
#include "rnetlib/verbs/verbs_client.h"
#include "rnetlib/verbs/verbs_event_loop.h"
#include "rnetlib/verbs/verbs_server.h"
#endif // RNETLIB_USE_RDMA

namespace rnetlib {

class RNetLib {
 public:
  enum Mode {
    SOCKET,
    VERBS
  };

  static std::unique_ptr<Client> NewClient(const std::string &addr, uint16_t port, Mode mode = Mode::SOCKET) {
#ifdef RNETLIB_USE_RDMA
    if (mode == VERBS) {
      return std::unique_ptr<Client>(new verbs::VerbsClient(addr, port));
    }
#endif // RNETLIB_USE_RDMA
    return std::unique_ptr<Client>(new socket::SocketClient(addr, port));
  }

  static std::unique_ptr<Server> NewServer(const std::string &addr, uint16_t port, Mode mode = Mode::SOCKET) {
#ifdef RNETLIB_USE_RDMA
    if (mode == VERBS) {
      return std::unique_ptr<Server>(new verbs::VerbsServer(addr, port));
    }
#endif // RNETLIB_USE_RDMA
    return std::unique_ptr<Server>(new socket::SocketServer(addr, port));
  }

  static std::unique_ptr<EventLoop> NewEventLoop(Mode mode = Mode::SOCKET) {
#ifdef RNETLIB_USE_RDMA
    if (mode == VERBS) {
      return std::unique_ptr<EventLoop>(new verbs::VerbsEventLoop);
    }
#endif // RNETLIB_USE_RDMA
    return std::unique_ptr<EventLoop>(new socket::SocketEventLoop);
  }
};

} // namespace rnetlib

#endif // RNETLIB_RNETLIB_H_
