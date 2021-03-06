#ifndef RNETLIB_SOCKET_SOCKET_CLIENT_H_
#define RNETLIB_SOCKET_SOCKET_CLIENT_H_

#include <poll.h>

#include <cerrno>
#include <chrono>
#include <future>

#include "rnetlib/client.h"
#include "rnetlib/socket/socket_channel.h"

#ifdef RNETLIB_ENABLE_VERBS
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
#endif // RNETLIB_ENABLE_VERBS

namespace rnetlib {
namespace socket {

class SocketClient : public Client, public SocketCommon, public EventHandler {
 public:
  explicit SocketClient(uint64_t self_desc) : self_desc_(self_desc) {}

  ~SocketClient() override = default;

  Channel::ptr Connect(const std::string &peer_addr, uint16_t peer_port, uint64_t peer_desc) override {
    auto addr_info = Open(peer_addr.c_str(), peer_port, 0);
    if (!addr_info) {
      // TODO: log error
      return nullptr;
    }

    auto ret = S_CONNECT(sock_fd_, S_DST_ADDR(addr_info), S_DST_ADDRLEN(addr_info));
    if (ret == -1) {
      Close();
      if (errno == ECONNREFUSED) {
        // the server is not ready yet.
        // TODO: log error
        return nullptr;
      } else {
        // unrecoverable error.
        // TODO: log error
        return nullptr;
      }
    }

    Channel::ptr ch(new SocketChannel(sock_fd_, peer_desc));
    ch->Send(&self_desc_, sizeof(self_desc_));

    // successfully connected.
    return std::move(ch);
  }

  std::future<Channel::ptr> Connect(const std::string &peer_addr, uint16_t peer_port,
                                    EventLoop &loop, std::function<void(Channel &)> on_established) {
    on_established_ = std::move(on_established);

    auto ret = NonBlockingConnect(peer_addr, peer_port);
    switch (ret) {
      case kConnSuccess:
        loop.AddHandler(*this);
        break;
      case kConnEstablished:
        OnEstablished();
        break;
      default:
        promise_.set_value(nullptr);
        break;
    }

    return promise_.get_future();
  }

  int OnEvent(int event_type, void *arg) override {
    if (event_type & POLLOUT) {
      return OnConnect();
    }

    return MAY_BE_REMOVED;
  }

  int OnError(int error_type) override {
    if (error_type & (POLLHUP | POLLERR)) {
      return OnConnect();
    }

    return MAY_BE_REMOVED;
  }

  void *GetHandlerID() const override {
    return const_cast<int *>(&sock_fd_);
  }

  short GetEventType() const override {
    return POLLOUT;
  }

 private:
  static const int kConnFailed = -1;
  static const int kConnSuccess = 0;
  static const int kConnEstablished = 1;
  uint64_t self_desc_;
  std::promise<Channel::ptr> promise_;
  std::function<void(Channel &)> on_established_;

  void OnEstablished() {
    std::unique_ptr<Channel> channel(new SocketChannel(sock_fd_));
    if (on_established_) {
      on_established_(*channel);
    }
    promise_.set_value(std::move(channel));
  }

  int NonBlockingConnect(const std::string &peer_addr, uint16_t peer_port) {
    auto addr_info = Open(peer_addr.c_str(), peer_port, 0);
    if (!addr_info) {
      // TODO: log error
      return kConnFailed;
    }

    // set socket to non-blocking mode
    SetNonBlocking(true);

    if (S_CONNECT(sock_fd_, S_DST_ADDR(addr_info), S_DST_ADDRLEN(addr_info)) == 0) {
      // this may happen when connection got established immediately.
      return kConnEstablished;
    } else {
      if (errno == EINPROGRESS) {
        // connection is expected to be established later.
        return kConnSuccess;
      } else {
        // unrecoverable error.
        // TODO: log error
        Close();
        return kConnFailed;
      }
    }
  }

  int OnConnect() {
    // check if connection has been established
    int val = 0;
    GetSockOpt(SOL_SOCKET, SO_ERROR, val);

    if (val == 0) {
      // connection has been established.
      OnEstablished();
    } else if (val == ECONNREFUSED) {
      // the peer is not ready yet.
      Close();
      promise_.set_value(nullptr);
    } else {
      // unrecoverable error.
      Close();
      promise_.set_value(nullptr);
    }

    return MAY_BE_REMOVED;
  }
};

} // namespace socket
} // namespace rnetlib

#endif // RNETLIB_SOCKET_SOCKET_CLIENT_H_
