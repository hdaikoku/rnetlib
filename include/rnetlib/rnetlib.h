#ifndef RNETLIB_RNETLIB_H_
#define RNETLIB_RNETLIB_H_

#include "rnetlib/client.h"
#include "rnetlib/event_loop.h"
#include "rnetlib/server.h"
#include "rnetlib/socket/socket_client.h"
#include "rnetlib/socket/socket_event_loop.h"
#include "rnetlib/socket/socket_server.h"

#ifdef USE_RDMA
#include "rnetlib/rdma/rdma_client.h"
#include "rnetlib/rdma/rdma_event_loop.h"
#include "rnetlib/rdma/rdma_server.h"
#endif // USE_RDMA

namespace rnetlib {

class RNetLib {
 public:
  enum Mode {
    SOCKET,
    VERBS
  };

  static std::unique_ptr<Client> NewClient(const std::string &addr, uint16_t port, Mode mode = Mode::SOCKET) {
#ifdef USE_RDMA
    if (mode == VERBS) {
      return std::unique_ptr<Client>(new rdma::RDMAClient(addr, port));
    }
#endif // USE_RDMA
    return std::unique_ptr<Client>(new socket::SocketClient(addr, port));
  }

  static std::unique_ptr<Server> NewServer(const std::string &addr, uint16_t port, Mode mode = Mode::SOCKET) {
#ifdef USE_RDMA
    if (mode == VERBS) {
      return std::unique_ptr<Server>(new rdma::RDMAServer(addr, port));
    }
#endif // USE_RDMA
    return std::unique_ptr<Server>(new socket::SocketServer(addr, port));
  }

  static std::unique_ptr<EventLoop> NewEventLoop(Mode mode = Mode::SOCKET) {
#ifdef USE_RDMA
    if (mode == VERBS) {
      return std::unique_ptr<EventLoop>(new rdma::RDMAEventLoop);
    }
#endif // USE_RDMA
    return std::unique_ptr<EventLoop>(new socket::SocketEventLoop);
  }
};

} // namespace rnetlib

#endif // RNETLIB_RNETLIB_H_
