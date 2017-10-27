#ifndef RNETLIB_SOCKET_SOCKET_CHANNEL_H_
#define RNETLIB_SOCKET_SOCKET_CHANNEL_H_

#include <limits.h>
#include <netdb.h>
#include <unistd.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

#include <string>

#include "rnetlib/channel.h"
#include "rnetlib/event_handler.h"
#include "rnetlib/socket/socket_common.h"
#include "rnetlib/socket/socket_local_memory_region.h"

#ifndef IOV_MAX
#define IOV_MAX 1024
#endif // IOV_MAX

namespace rnetlib {
namespace socket {

class SocketChannel : public Channel, public EventHandler, public SocketCommon {
 public:
  SocketChannel() = default;
  SocketChannel(int sock_fd) : SocketCommon(sock_fd) {}

  bool SetNonBlocking(bool non_blocking) override {
    return SocketCommon::SetNonBlocking(non_blocking);
  }

  size_t Send(void *buf, size_t len) override {
    struct iovec iov;
    iov.iov_base = buf;
    iov.iov_len = len;

    return (SendIOV(&iov, 1) == 1) ? len : 0;
  }

  size_t Recv(void *buf, size_t len) override {
    struct iovec iov;
    iov.iov_base = buf;
    iov.iov_len = len;

    return (RecvIOV(&iov, 1) == 1) ? len : 0;
  }

  size_t Send(const LocalMemoryRegion &mem) override {
    return Send(mem.GetAddr(), mem.GetLength());
  }

  size_t Recv(const LocalMemoryRegion &mem) override {
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

  size_t SendV(const std::vector<std::unique_ptr<LocalMemoryRegion>> &mrs) override {
    std::vector<struct iovec> iov;
    size_t total_len = 0;
    for (const auto &mr : mrs) {
      total_len += mr->GetLength();
      iov.push_back({mr->GetAddr(), mr->GetLength()});
    }

    return (SendIOV(iov.data(), iov.size())) == iov.size() ? total_len : 0;
  }

  size_t RecvV(const std::vector<std::unique_ptr<LocalMemoryRegion>> &mrs) override {
    std::vector<struct iovec> iov;
    size_t total_len = 0;
    for (const auto &mr : mrs) {
      total_len += mr->GetLength();
      iov.push_back({mr->GetAddr(), mr->GetLength()});
    }

    return (RecvIOV(iov.data(), iov.size())) == iov.size() ? total_len : 0;
  }

  size_t ISendV(const std::vector<std::unique_ptr<LocalMemoryRegion>> &vec, EventLoop &evloop) override {
    // FIXME: check if this sock_fd is set to non-blocking mode.
    if (!vec.empty()) {
      for (const auto &mr : vec) {
        send_iov_.push_back({mr->GetAddr(), mr->GetLength()});
      }
      evloop.AddHandler(*this);
    }
    return vec.size();
  }

  size_t IRecvV(const std::vector<std::unique_ptr<LocalMemoryRegion>> &vec, EventLoop &evloop) override {
    // FIXME: check if this sock_fd is set to non-blocking mode.
    if (!vec.empty()) {
      for (const auto &mr : vec) {
        recv_iov_.push_back({mr->GetAddr(), mr->GetLength()});
      }
      evloop.AddHandler(*this);
    }
    return vec.size();
  }

  size_t Write(const LocalMemoryRegion &local_mem, const RemoteMemoryRegion &remote_mem) override {
    return Send(local_mem.GetAddr(), local_mem.GetLength());
  }

  size_t Read(const LocalMemoryRegion &local_mem, const RemoteMemoryRegion &remote_mem) override {
    return Recv(local_mem.GetAddr(), local_mem.GetLength());
  }

  std::unique_ptr<LocalMemoryRegion> RegisterMemory(void *addr, size_t len, int type) const override {
    return std::unique_ptr<LocalMemoryRegion>(new SocketLocalMemoryRegion(addr, len));
  }

  void SynRemoteMemoryRegions(const LocalMemoryRegion::ptr *lmr, size_t len) override {}

  void AckRemoteMemoryRegions(RemoteMemoryRegion *rmr, size_t len) override {}

  int OnEvent(int event_type, void *arg) override {
    if (event_type & POLLOUT) {
      auto offset = SendIOV(send_iov_.data(), send_iov_.size());
      if (offset > 0) {
        send_iov_.erase(send_iov_.begin(), send_iov_.begin() + offset);
      }
    }
    if (event_type & POLLIN) {
      auto offset = RecvIOV(recv_iov_.data(), recv_iov_.size());
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

  size_t SendIOV(struct iovec *iov, size_t iovcnt) const {
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

  size_t RecvIOV(struct iovec *iov, size_t iovcnt) const {
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

} // namespace socket
} // namespace rnetlib

#endif // RNETLIB_SOCKET_SOCKET_CHANNEL_H_
