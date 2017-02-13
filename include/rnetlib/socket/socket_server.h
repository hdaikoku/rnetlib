//
// Created by Harunobu Daikoku on 2017/02/10.
//

#ifndef RNETLIB_SOCKET_SOCKET_SERVER_H
#define RNETLIB_SOCKET_SOCKET_SERVER_H

#include <cerrno>

#include "rnetlib/server.h"
#include "rnetlib/socket/socket_channel.h"

#ifdef USE_RDMA
// rsocket-specific functions
#define S_BIND(s, a, l)   rbind(s, a, l)
#define S_LISTEN(s, b)    rlisten(s, b)
#define S_ACCEPT(s, a, l) raccept(s, a, l)
#define S_SRC_ADDR(a)     a->ai_src_addr
#define S_SRC_ADDRLEN(a)  a->ai_src_len
#define S_PASSIVE         RAI_PASSIVE
#else
// BSD socket-specific functions
#define S_BIND(s, a, l)   bind(s, a, l)
#define S_LISTEN(s, b)    listen(s, b)
#define S_ACCEPT(s, a, l) accept(s, a, l)
#define S_SRC_ADDR(a)     a->ai_addr
#define S_SRC_ADDRLEN(a)  a->ai_addrlen
#define S_PASSIVE         AI_PASSIVE
#endif

namespace rnetlib {
namespace socket {
class SocketServer : public Server, public SocketCommon {
 public:
  bool Listen(uint16_t server_port) override {
    auto result = Init(nullptr, server_port, S_PASSIVE);
    if (!result) {
      return false;
    }

    if (!SetSockOpt(SOL_SOCKET, SO_REUSEADDR, 1)) {
      return false;
    }

    if (S_BIND(sock_fd_, S_SRC_ADDR(result), S_SRC_ADDRLEN(result)) == -1) {
      // TODO: log error
      return false;
    }

    if (S_LISTEN(sock_fd_, 1024) == -1) {
      // TODO: log error
      return false;
    }

    return true;
  }

  std::unique_ptr<Channel> Accept() override {
    auto sock_fd = S_ACCEPT(sock_fd_, NULL, NULL);
    if (sock_fd < 0) {
      if (errno != EWOULDBLOCK) {
        // TODO: log error
      }
      return nullptr;
    }

    return std::unique_ptr<Channel>(new SocketChannel(sock_fd));
  }

};
}
}

#endif //RNETLIB_SOCKET_SOCKET_SERVER_H
