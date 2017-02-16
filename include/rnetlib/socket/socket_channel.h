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
#include "rnetlib/socket/socket_local_memory_region.h"

namespace rnetlib {
namespace socket {
class SocketChannel : public Channel, public SocketCommon {
 public:

  SocketChannel() {}

  SocketChannel(int sock_fd) : SocketCommon(sock_fd) {}

  bool SetNonBlocking(bool non_blocking) override {
    return SocketCommon::SetNonBlocking(non_blocking);
  }

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

  size_t Send(LocalMemoryRegion &mem) const override {
    return Send(mem.GetAddr(), mem.GetLength());
  }

  size_t Recv(LocalMemoryRegion &mem) const override {
    return Recv(mem.GetAddr(), mem.GetLength());
  }

  size_t Write(LocalMemoryRegion &local_mem, RemoteMemoryRegion &remote_mem) const override {
    return Send(local_mem.GetAddr(), local_mem.GetLength());
  }

  size_t Read(LocalMemoryRegion &local_mem, RemoteMemoryRegion &remote_mem) const override {
    return Recv(local_mem.GetAddr(), local_mem.GetLength());
  }

  size_t PollWrite(LocalMemoryRegion &local_mem) const override {
    return Recv(local_mem.GetAddr(), local_mem.GetLength());
  }

  size_t PollRead(LocalMemoryRegion &local_mem) const override {
    return Send(local_mem.GetAddr(), local_mem.GetLength());
  }

  std::unique_ptr<LocalMemoryRegion> RegisterMemory(void *addr, size_t len, int type) const override {
    return std::unique_ptr<LocalMemoryRegion>(new SocketLocalMemoryRegion(addr, len));
  }

  void SynRemoteMemoryRegion(LocalMemoryRegion &mem) const override {

  }

  std::unique_ptr<RemoteMemoryRegion> AckRemoteMemoryRegion() const override {
    return std::unique_ptr<RemoteMemoryRegion>(new RemoteMemoryRegion(nullptr, 0));;
  }

 private:
  std::string peer_addr_;
  uint16_t peer_port_;

};
}
}

#endif //RNETLIB_SOCKET_SOCKET_CHANNEL_H
