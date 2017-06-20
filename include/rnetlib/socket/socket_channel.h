//
// Created by Harunobu Daikoku on 2017/02/10.
//

#ifndef RNETLIB_SOCKET_SOCKET_CHANNEL_H
#define RNETLIB_SOCKET_SOCKET_CHANNEL_H

#include <deque>
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
    isend_ctxs_.push_back({buf, len, 0});
    evloop.AddHandler(*this);
    return len;
  }

  size_t IRecv(void *buf, size_t len, EventLoop &evloop) override {
    // FIXME: check if this sock_fd is set to non-blocking mode.
    irecv_ctxs_.push_back({buf, len, 0});
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
      while (isend_ctxs_.size() > 0) {
        auto &ctx = isend_ctxs_.front();
        ctx.offset += SendSome(ctx);
        if (ctx.offset == ctx.len) {
          isend_ctxs_.pop_front();
        } else {
          break;
        }
      }
    }
    if (event_type & POLLIN) {
      while (irecv_ctxs_.size() > 0) {
        auto &ctx = irecv_ctxs_.front();
        ctx.offset += RecvSome(ctx);
        if (ctx.offset == ctx.len) {
          irecv_ctxs_.pop_front();
        } else {
          break;
        }
      }
    }

    return (isend_ctxs_.empty() && irecv_ctxs_.empty()) ? MAY_BE_REMOVED : 0;
  }

  int OnError(int error_type) override {
    return MAY_BE_REMOVED;
  }

  void *GetHandlerID() const override {
    return const_cast<int *>(&sock_fd_);
  }

  short GetEventType() const override {
    short type = 0;

    if (isend_ctxs_.size() > 0) {
      type |= POLLOUT;
    }
    if (irecv_ctxs_.size() > 0) {
      type |= POLLIN;
    }

    return type;
  }

 private:
  struct non_blocking_context {
    void *buf;
    size_t len;
    size_t offset;
  };

  std::string peer_addr_;
  uint16_t peer_port_;
  std::deque<struct non_blocking_context> isend_ctxs_;
  std::deque<struct non_blocking_context> irecv_ctxs_;

  size_t SendSome(struct non_blocking_context &ctx) const {
    size_t len = ctx.len;
    size_t offset = ctx.offset;

    while (offset < len) {
      auto sent = S_SEND(sock_fd_, static_cast<char *>(ctx.buf) + offset, len - offset, 0);
      if (sent < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          // send buffer is full
          break;
        }
        // error
        return 0;
      } else {
        offset += sent;
      }
    }

    return (offset - ctx.offset);
  }

  size_t RecvSome(struct non_blocking_context &ctx) const {
    size_t len = ctx.len;
    size_t offset = ctx.offset;

    while (offset < len) {
      auto recvd = S_RECV(sock_fd_, static_cast<char *>(ctx.buf) + offset, len - offset, 0);
      if (recvd < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
          // no more data to receive
          break;
        }
        // something went wrong
        return 0;
      } else if (recvd == 0) {
        // connection closed by client
        return 0;
      }
      offset += recvd;
    }

    return (offset - ctx.offset);
  }

};
}
}

#endif //RNETLIB_SOCKET_SOCKET_CHANNEL_H
