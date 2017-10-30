#ifndef RNETLIB_SOCKET_SOCKET_CHANNEL_H_
#define RNETLIB_SOCKET_SOCKET_CHANNEL_H_

#include <netdb.h>
#include <unistd.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

#include <climits>
#include <string>
#include <vector>

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
    return Send(RegisterMemory(buf, len, MR_LOCAL_READ));
  }

  size_t Recv(void *buf, size_t len) override {
    return Recv(RegisterMemory(buf, len, MR_LOCAL_WRITE));
  }

  size_t Send(const LocalMemoryRegion::ptr &lmr) override {
    return SendV(&lmr, 1);
  }

  size_t Recv(const LocalMemoryRegion::ptr &lmr) override {
    return RecvV(&lmr, 1);
  }

  size_t ISend(void *buf, size_t len, EventLoop &evloop) override {
    auto lmr = RegisterMemory(buf, len, MR_LOCAL_READ);
    return ISendV(&lmr, 1, evloop);
  }

  size_t IRecv(void *buf, size_t len, EventLoop &evloop) override {
    auto lmr = RegisterMemory(buf, len, MR_LOCAL_WRITE);
    return IRecvV(&lmr, 1, evloop);
  }

  size_t SendV(const LocalMemoryRegion::ptr *lmr, size_t lmrcnt) override {
    std::vector<struct iovec> iov(lmrcnt);
    size_t total_len = 0;
    for (size_t i = 0; i < lmrcnt; i++) {
      total_len += lmr[i]->GetLength();
      iov[i] = {lmr[i]->GetAddr(), lmr[i]->GetLength()};
    }

    return (SendIOV(iov.data(), iov.size()) == iov.size()) ? total_len : 0;
  }

  size_t RecvV(const LocalMemoryRegion::ptr *lmr, size_t lmrcnt) override {
    std::vector<struct iovec> iov(lmrcnt);
    size_t total_len = 0;
    for (size_t i = 0; i < lmrcnt; i++) {
      total_len += lmr[i]->GetLength();
      iov[i] = {lmr[i]->GetAddr(), lmr[i]->GetLength()};
    }

    return (RecvIOV(iov.data(), iov.size()) == iov.size()) ? total_len : 0;
  }

  size_t ISendV(const LocalMemoryRegion::ptr *lmr, size_t lmrcnt, EventLoop &evloop) override {
    // FIXME: check if this sock_fd is set to non-blocking mode.
    if (lmrcnt > 0) {
      for (size_t i = 0; i < lmrcnt; i++) {
        send_iov_.push_back({lmr[i]->GetAddr(), lmr[i]->GetLength()});
      }
      evloop.AddHandler(*this);
    }

    return lmrcnt;
  }

  size_t IRecvV(const LocalMemoryRegion::ptr *lmr, size_t lmrcnt, EventLoop &evloop) override {
    // FIXME: check if this sock_fd is set to non-blocking mode.
    if (lmrcnt > 0) {
      for (size_t i = 0; i < lmrcnt; i++) {
        recv_iov_.push_back({lmr[i]->GetAddr(), lmr[i]->GetLength()});
      }
      evloop.AddHandler(*this);
    }

    return lmrcnt;
  }

  size_t Write(const LocalMemoryRegion::ptr &lmr, const RemoteMemoryRegion &rmr) override {
    return WriteV(&lmr, &rmr, 1);
  }

  size_t Read(const LocalMemoryRegion::ptr &lmr, const RemoteMemoryRegion &rmr) override {
    return ReadV(&lmr, &rmr, 1);
  }

  size_t WriteV(const LocalMemoryRegion::ptr *lmr, const RemoteMemoryRegion *rmr, size_t cnt) override {
    // TODO: implement this method.
    return 0;
  }

  size_t ReadV(const LocalMemoryRegion::ptr *lmr, const RemoteMemoryRegion *rmr, size_t cnt) override {
    // TODO: implement this method.
    return 0;
  }

  LocalMemoryRegion::ptr RegisterMemory(void *addr, size_t len, int type) const override {
    return LocalMemoryRegion::ptr(new SocketLocalMemoryRegion(addr, len));
  }

  void SynRemoteMemoryRegionV(const LocalMemoryRegion::ptr *lmr, size_t lmrcnt) override {}

  void AckRemoteMemoryRegionV(RemoteMemoryRegion *rmr, size_t rmrcnt) override {}

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

    if (!send_iov_.empty()) {
      type |= POLLOUT;
    }
    if (!recv_iov_.empty()) {
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
