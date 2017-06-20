//
// Created by Harunobu Daikoku on 2017/02/10.
//

#ifndef RNETLIB_SOCKET_SOCKET_CHANNEL_H
#define RNETLIB_SOCKET_SOCKET_CHANNEL_H

#include <limits.h>
#include <netdb.h>
#include <string>
#include <unistd.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

#include "rnetlib/channel.h"
#include "rnetlib/event_handler.h"
#include "rnetlib/socket/socket_common.h"
#include "rnetlib/socket/socket_local_memory_region.h"

#ifndef IOV_MAX
#define IOV_MAX 1024
#endif //IOV_MAX

namespace rnetlib {
namespace socket {
class SocketChannel : public Channel, public EventHandler, public SocketCommon {
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

  size_t ISend(void *buf, size_t len, EventLoop &evloop) override {
    // FIXME: check if this sock_fd is set to non-blocking mode.
    send_iov_.push_back({buf, len});
    evloop.AddHandler(*this);
    return len;
  }

  size_t IRecv(void *buf, size_t len, EventLoop &evloop) override {
    // FIXME: check if this sock_fd is set to non-blocking mode.
    recv_iov_.push_back({buf, len});
    evloop.AddHandler(*this);
    return len;
  }

  size_t SendSG(const std::vector<std::unique_ptr<LocalMemoryRegion>> &mrs) const override {
    struct iovec iov[mrs.size()];
    int iovcnt = 0, offset = 0;
    size_t total_sent = 0;

    for (const auto &mr : mrs) {
      iov[iovcnt].iov_base = mr->GetAddr();
      iov[iovcnt].iov_len = mr->GetLength();
      iovcnt++;
    }

    while (iovcnt > offset) {
      auto sent = S_WRITEV(sock_fd_, iov + offset, (iovcnt - offset) > IOV_MAX ? IOV_MAX : (iovcnt - offset));
      if (sent > 0) {
        total_sent += sent;
        while (offset < iovcnt) {
          if (iov[offset].iov_len > sent) {
            iov[offset].iov_base = static_cast<char *>(iov[offset].iov_base) + sent;
            iov[offset].iov_len -= sent;
            break;
          }
          sent -= iov[offset].iov_len;
          offset++;
        }
      } else {
        // TODO: handle error
        return 0;
      }
    }

    return total_sent;
  }

  size_t RecvSG(const std::vector<std::unique_ptr<LocalMemoryRegion>> &mrs) const override {
    struct iovec iov[mrs.size()];
    int iovcnt = 0, offset = 0;
    size_t total_recvd = 0;

    for (const auto &mr : mrs) {
      iov[iovcnt].iov_base = mr->GetAddr();
      iov[iovcnt].iov_len = mr->GetLength();
      iovcnt++;
    }

    while (iovcnt > offset) {
      auto recvd = S_READV(sock_fd_, iov + offset, (iovcnt - offset) > IOV_MAX ? IOV_MAX : (iovcnt - offset));
      if (recvd > 0) {
        total_recvd += recvd;
        while (offset < iovcnt) {
          if (iov[offset].iov_len > recvd) {
            iov[offset].iov_base = static_cast<char *>(iov[offset].iov_base) + recvd;
            iov[offset].iov_len -= recvd;
            break;
          }
          recvd -= iov[offset].iov_len;
          offset++;
        }
      } else {
        // TODO: handle error
        return 0;
      }
    }

    return total_recvd;
  }

  size_t ISendVec(const std::vector<std::unique_ptr<LocalMemoryRegion>> &vec, EventLoop &evloop) override {
    for (const auto &mr : vec) {
      send_iov_.push_back({mr->GetAddr(), mr->GetLength()});
    }
    evloop.AddHandler(*this);
    return vec.size();
  }

  size_t IRecvVec(const std::vector<std::unique_ptr<LocalMemoryRegion>> &vec, EventLoop &evloop) override {
    for (const auto &mr : vec) {
      recv_iov_.push_back({mr->GetAddr(), mr->GetLength()});
    }
    evloop.AddHandler(*this);
    return vec.size();
  }

  size_t Write(LocalMemoryRegion &local_mem, RemoteMemoryRegion &remote_mem) const override {
    return Send(local_mem.GetAddr(), local_mem.GetLength());
  }

  size_t Read(LocalMemoryRegion &local_mem, RemoteMemoryRegion &remote_mem) const override {
    return Recv(local_mem.GetAddr(), local_mem.GetLength());
  }

  std::unique_ptr<LocalMemoryRegion> RegisterMemory(void *addr, size_t len, int type) const override {
    return std::unique_ptr<LocalMemoryRegion>(new SocketLocalMemoryRegion(addr, len));
  }

  void SynRemoteMemoryRegion(LocalMemoryRegion &mem) const override {

  }

  std::unique_ptr<RemoteMemoryRegion> AckRemoteMemoryRegion() const override {
    return std::unique_ptr<RemoteMemoryRegion>(new RemoteMemoryRegion(nullptr, 0, 0));;
  }

  int OnEvent(int event_type, void *arg) override {
    if (event_type & POLLOUT) {
      auto offset = SendSome(send_iov_.data(), send_iov_.size());
      if (offset > 0) {
        send_iov_.erase(send_iov_.begin(), send_iov_.begin() + offset);
      }
    }
    if (event_type & POLLIN) {
      auto offset = RecvSome(recv_iov_.data(), recv_iov_.size());
      if (offset > 0) {
        recv_iov_.erase(recv_iov_.begin(), recv_iov_.begin() + offset);
      }
    }

    return (send_iov_.empty() && recv_iov_.empty()) ? MAY_BE_REMOVED : 0;
  }

  int OnError(int error_type) override {
    return MAY_BE_REMOVED;
  }

  void *GetHandlerID() const override {
    return const_cast<int *>(&sock_fd_);
  }

  short GetEventType() const override {
    short type = 0;

    if (send_iov_.size() > 0) {
      type |= POLLOUT;
    }
    if (recv_iov_.size() > 0) {
      type |= POLLIN;
    }

    return type;
  }

 private:
  std::string peer_addr_;
  uint16_t peer_port_;
  std::vector<struct iovec> send_iov_;
  std::vector<struct iovec> recv_iov_;

  size_t SendSome(struct iovec *iov, size_t iovcnt) const {
    size_t offset = 0;

    while (iovcnt > offset) {
      auto sent = S_WRITEV(sock_fd_, iov + offset, (iovcnt - offset) > IOV_MAX ? IOV_MAX : (iovcnt - offset));
      if (sent > 0) {
        while (offset < iovcnt) {
          if (iov[offset].iov_len > sent) {
            iov[offset].iov_base = static_cast<char *>(iov[offset].iov_base) + sent;
            iov[offset].iov_len -= sent;
            break;
          }
          sent -= iov[offset].iov_len;
          offset++;
        }
      } else {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          // SNDBUF is full.
          break;
        }
        // TODO: handle error
        return 0;
      }
    }

    return offset;
  }

  size_t RecvSome(struct iovec *iov, size_t iovcnt) const {
    size_t offset = 0;

    while (iovcnt > offset) {
      auto recvd = S_READV(sock_fd_, iov + offset, (iovcnt - offset) > IOV_MAX ? IOV_MAX : (iovcnt - offset));
      if (recvd > 0) {
        while (offset < iovcnt) {
          if (iov[offset].iov_len > recvd) {
            iov[offset].iov_base = static_cast<char *>(iov[offset].iov_base) + recvd;
            iov[offset].iov_len -= recvd;
            break;
          }
          recvd -= iov[offset].iov_len;
          offset++;
        }
      } else {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
          // RCVBUF is empty.
          break;
        }
        // TODO: handle error
        return 0;
      }
    }

    return offset;
  }

};
}
}

#endif //RNETLIB_SOCKET_SOCKET_CHANNEL_H
