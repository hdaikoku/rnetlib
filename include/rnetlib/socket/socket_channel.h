//
// Created by Harunobu Daikoku on 2017/02/10.
//

#ifndef RNETLIB_SOCKET_SOCKET_CHANNEL_H
#define RNETLIB_SOCKET_SOCKET_CHANNEL_H

#include <netdb.h>
#include <string>
#include <unistd.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

#include "rnetlib/channel.h"
#include "rnetlib/socket/socket_common.h"
#include "rnetlib/socket/socket_registered_memory.h"

namespace rnetlib {
namespace socket {
class SocketChannel : public Channel, public SocketCommon {
 public:

  SocketChannel() {}

  SocketChannel(int sock_fd) : SocketCommon(sock_fd) {}

  size_t Send(void *buf, size_t len) const override {
    size_t offset = 0;

    while (offset < len) {
      auto sent = S_WRITE(sock_fd_, (char *) buf + offset, len - offset);
      if (sent > 0) {
        offset += sent;
      } else {
        // TODO: handle error
        return 0;
      }
    }

    return offset;
  }

  size_t Recv(void *buf, size_t len) const override {
    size_t offset = 0;

    while (offset < len) {
      auto recvd = S_READ(sock_fd_, (char *) buf + offset, len - offset);
      if (recvd > 0) {
        offset += recvd;
      } else {
        // TODO: handle error
        return 0;
      }
    }

    return offset;
  }

  size_t Send(RegisteredMemory &mem) const override {
    return Send(mem.GetAddr(), mem.GetLength());
  }
  size_t Recv(RegisteredMemory &mem) const override {
    return Recv(mem.GetAddr(), mem.GetLength());
  }

  std::unique_ptr<RegisteredMemory> RegisterMemory(void *addr, size_t len) const override {
    return std::unique_ptr<RegisteredMemory>(new SocketRegisteredMemory(addr, len));
  }

 private:
  std::string peer_addr_;
  uint16_t peer_port_;

};
}
}

#endif //RNETLIB_SOCKET_SOCKET_CHANNEL_H
