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
#include "rnetlib/socket/socket_event_loop.h"
#include "rnetlib/socket/socket_local_memory_region.h"

#ifndef IOV_MAX
#define IOV_MAX 1024
#endif // IOV_MAX

namespace rnetlib {
namespace socket {

class SocketChannel : public Channel, public EventHandler, public SocketCommon {
 public:
  explicit SocketChannel(int sock_fd) : SocketChannel(sock_fd, 0) {}
  SocketChannel(int sock_fd, uint64_t peer_desc)
      : SocketCommon(sock_fd), peer_desc_(peer_desc), evloop_(new SocketEventLoop) { SetNonBlocking(true); }

  uint64_t GetDesc() const override { return peer_desc_; }

  size_t Send(void *buf, size_t len) override { return Send(RegisterMemoryRegion(buf, len, MR_LOCAL_READ)); }

  size_t Recv(void *buf, size_t len) override { return Recv(RegisterMemoryRegion(buf, len, MR_LOCAL_WRITE)); }

  size_t Send(const LocalMemoryRegion::ptr &lmr) override { return SendV(&lmr, 1); }

  size_t Recv(const LocalMemoryRegion::ptr &lmr) override { return RecvV(&lmr, 1); }

  size_t ISend(void *buf, size_t len, const EventLoop::ptr &evloop) override {
    auto lmr = RegisterMemoryRegion(buf, len, MR_LOCAL_READ);
    return ISendV(&lmr, 1, evloop);
  }

  size_t IRecv(void *buf, size_t len, const EventLoop::ptr &evloop) override {
    auto lmr = RegisterMemoryRegion(buf, len, MR_LOCAL_WRITE);
    return IRecvV(&lmr, 1, evloop);
  }

  size_t SendV(const LocalMemoryRegion::ptr *lmr, size_t lmrcnt) override {
    auto ret = ISendV(lmr, lmrcnt, evloop_);
    evloop_->WaitAll(-1);
    return ret;
  }

  size_t RecvV(const LocalMemoryRegion::ptr *lmr, size_t lmrcnt) override {
    auto ret = IRecvV(lmr, lmrcnt, evloop_);
    evloop_->WaitAll(-1);
    return ret;
  }

  size_t ISendV(const LocalMemoryRegion::ptr *lmr, size_t lmrcnt, const EventLoop::ptr &evloop) override {
    size_t total_len = 0;
    for (size_t i = 0; i < lmrcnt; i++) {
      auto len = lmr[i]->GetLength();
      total_len += len;
      if (len > 0) {
        send_iov_.push_back({lmr[i]->GetAddr(), lmr[i]->GetLength()});
      }
    }

    if (!send_iov_.empty()) {
      evloop->AddHandler(*this);
    }

    return total_len;
  }

  size_t IRecvV(const LocalMemoryRegion::ptr *lmr, size_t lmrcnt, const EventLoop::ptr &evloop) override {
    size_t total_len = 0;
    for (size_t i = 0; i < lmrcnt; i++) {
      auto len = lmr[i]->GetLength();
      total_len += len;
      if (len > 0) {
        recv_iov_.push_back({lmr[i]->GetAddr(), len});
      }
    }

    if (!recv_iov_.empty()) {
      evloop->AddHandler(*this);
    }

    return total_len;
  }

  size_t Write(void *buf, size_t len, const RemoteMemoryRegion &rmr) override {
    return Write(RegisterMemoryRegion(buf, len, MR_LOCAL_READ), rmr);
  }

  size_t Read(void *buf, size_t len, const RemoteMemoryRegion &rmr) override {
    return Read(RegisterMemoryRegion(buf, len, MR_LOCAL_WRITE), rmr);
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

  LocalMemoryRegion::ptr RegisterMemoryRegion(void *addr, size_t len, int type) const override {
    return LocalMemoryRegion::ptr(new SocketLocalMemoryRegion(addr, len));
  }

  void SynRemoteMemoryRegionV(const LocalMemoryRegion::ptr *lmr, size_t lmrcnt) override {
    std::vector<RemoteMemoryRegion> rmrs;
    rmrs.reserve(lmrcnt);

    for (auto i = 0; i < lmrcnt; i++) {
      rmrs.emplace_back(*lmr[i]);
    }

    Send(rmrs.data(), sizeof(RemoteMemoryRegion) * rmrs.size());
  }

  void AckRemoteMemoryRegionV(RemoteMemoryRegion *rmr, size_t rmrcnt) override {
    Recv(rmr, sizeof(RemoteMemoryRegion) * rmrcnt);
  }

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

  int OnError(int error_type) override { return MAY_BE_REMOVED; }

  void *GetHandlerID() const override { return const_cast<int *>(&sock_fd_); }

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

  void SetDesc(uint64_t peer_desc) { peer_desc_ = peer_desc; }

 private:
  uint64_t peer_desc_;
  std::vector<struct iovec> send_iov_;
  std::vector<struct iovec> recv_iov_;
  EventLoop::ptr evloop_;

  size_t SendIOV(struct iovec *iov, size_t iovcnt) const {
    size_t offset = 0;

    while (offset < iovcnt) {
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

    while (offset < iovcnt) {
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
