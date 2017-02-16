//
// Created by Harunobu Daikoku on 2017/02/15.
//

#ifndef RNETLIB_RNETLIB_H
#define RNETLIB_RNETLIB_H

#include "rnetlib/client.h"
#include "rnetlib/event_loop.h"
#include "rnetlib/server.h"
#include "rnetlib/socket/socket_client.h"
#include "rnetlib/socket/socket_event_loop.h"
#include "rnetlib/socket/socket_server.h"

#ifdef USE_RDMA
#include "rnetlib/rdma/rdma_client.h"
#include "rnetlib/rdma/rdma_server.h"
#endif //USE_RDMA

namespace rnetlib {
class RNetLib {
 public:

  enum Mode {
    SOCKET,
    VERBS
  };

  static void SetMode(Mode mode) {
    mode_ = mode;
  }

  static std::unique_ptr<Client> NewClient(const std::string &addr, uint16_t port) {
#ifdef USE_RDMA
    if (mode_ == VERBS) {
      return std::unique_ptr<Client>(new rdma::RDMAClient(addr, port));
    } else {
      return std::unique_ptr<Client>(new socket::SocketClient(addr, port));
    }
#else
    return std::unique_ptr<Client>(new socket::SocketClient(addr, port));
#endif //USE_RDMA
  }

  static std::unique_ptr<Server> NewServer(const std::string &addr, uint16_t port) {
#ifdef USE_RDMA
    if (mode_ == VERBS) {
      return std::unique_ptr<Server>(new rdma::RDMAServer(addr, port));
    } else {
      return std::unique_ptr<Server>(new socket::SocketServer(addr, port));
    }
#else
    return std::unique_ptr<Server>(new socket::SocketServer(addr, port));
#endif //USE_RDMA
  }

  static std::unique_ptr<EventLoop> NewEventLoop() {
#ifdef USE_RDMA
    if (mode_ == VERBS) {
      return std::unique_ptr<EventLoop>(new rdma::RDMAEventLoop);
    } else {
      return std::unique_ptr<EventLoop>(new socket::SocketEventLoop);
    }
#else
    return std::unique_ptr<EventLoop>(new socket::SocketEventLoop);
#endif //USE_RDMA
  }

 private:
  static Mode mode_;

};
// definition of static member
RNetLib::Mode RNetLib::mode_ = SOCKET;
}

#endif //RNETLIB_RNETLIB_H
