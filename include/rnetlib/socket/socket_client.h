//
// Created by Harunobu Daikoku on 2017/02/10.
//

#ifndef RNETLIB_SOCKET_SOCKET_CLIENT_H
#define RNETLIB_SOCKET_SOCKET_CLIENT_H

#include <cerrno>
#include <chrono>
#include <thread>

#include "rnetlib/client.h"
#include "rnetlib/socket/socket_channel.h"

#ifdef USE_RDMA
// rsocket-specific functions
#include <rdma/rsocket.h>

#define S_CONNECT(s, a, l)           rconnect(s, a, l)
#define S_DST_ADDR(a)                a->ai_dst_addr
#define S_DST_ADDRLEN(a)             a->ai_dst_len
#else
// BSD-socket specific functions
#define S_CONNECT(s, a, l)           connect(s, a, l)
#define S_DST_ADDR(a)                a->ai_addr
#define S_DST_ADDRLEN(a)             a->ai_addrlen
#endif //USE_RDMA

namespace rnetlib {
namespace socket {
class SocketClient : public Client, public SocketCommon {
 public:

  SocketClient(const std::string &peer_addr, uint16_t peer_port)
      : peer_addr_(peer_addr), peer_port_(peer_port) {}

  virtual ~SocketClient() {}

  std::unique_ptr<Channel> Connect() override {
    // TODO: timeout and backoff should be user-configurable
    auto timeout = std::chrono::minutes(3);
    int backoff_msecs = 1;

    auto start = std::chrono::steady_clock::now();
    while (true) {
      if (start + timeout < std::chrono::steady_clock::now()) {
        // timed-out
        // TODO: log error
        return nullptr;
      }

      auto addr_info = Init(peer_addr_.c_str(), peer_port_, 0);
      if (!addr_info) {
        // TODO: log error
        return nullptr;
      }

      auto ret = S_CONNECT(sock_fd_, S_DST_ADDR(addr_info), S_DST_ADDRLEN(addr_info));
      if (ret == -1) {
        S_CLOSE(sock_fd_);
        if (errno == ECONNREFUSED) {
          // the server is not ready yet.
          std::this_thread::sleep_for(std::chrono::milliseconds(backoff_msecs));
          backoff_msecs = std::min(1000, backoff_msecs * 2);
          continue;
        } else {
          // unrecoverable error.
          // TODO: log error
          return nullptr;
        }
      } else {
        // successfully connected.
        return std::unique_ptr<Channel>(new SocketChannel(sock_fd_));
      }
    }
  }

 private:
  std::string peer_addr_;
  uint16_t peer_port_;

};
}
}

#endif //RNETLIB_SOCKET_SOCKET_CLIENT_H
