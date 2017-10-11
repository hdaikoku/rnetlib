#ifndef RNETLIB_SOCKET_SOCKET_SERVER_H_
#define RNETLIB_SOCKET_SOCKET_SERVER_H_

#include <poll.h>

#include <cerrno>

#include "rnetlib/event_handler.h"
#include "rnetlib/server.h"
#include "rnetlib/socket/socket_channel.h"

#ifdef USE_RDMA
// rsocket-specific functions
#define S_ACCEPT(s, a, l)       raccept(s, a, l)
#define S_BIND(s, a, l)         rbind(s, a, l)
#define S_GETSOCKNAME(s, n, l)  rgetsockname(s, n, l)
#define S_LISTEN(s, b)          rlisten(s, b)
#define S_PASSIVE               RAI_PASSIVE
#define S_SRC_ADDR(a)           a->ai_src_addr
#define S_SRC_ADDRLEN(a)        a->ai_src_len
#else
// BSD socket-specific functions
#define S_ACCEPT(s, a, l)       accept(s, a, l)
#define S_BIND(s, a, l)         bind(s, a, l)
#define S_GETSOCKNAME(s, n, l)  getsockname(s, n, l)
#define S_LISTEN(s, b)          listen(s, b)
#define S_PASSIVE               AI_PASSIVE
#define S_SRC_ADDR(a)           a->ai_addr
#define S_SRC_ADDRLEN(a)        a->ai_addrlen
#endif // USE_RDMA

namespace rnetlib {
namespace socket {

class SocketServer : public Server, public SocketCommon, public EventHandler {
 public:
  SocketServer(const std::string &bind_addr, uint16_t bind_port)
      : bind_addr_(bind_addr), bind_port_(bind_port) {}

  bool Listen() override {
    auto result = Open(bind_addr_.c_str(), bind_port_, S_PASSIVE);
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

  Channel::Ptr Accept() override {
    auto sock_fd = S_ACCEPT(sock_fd_, NULL, NULL);
    if (sock_fd < 0) {
      if (errno != EWOULDBLOCK) {
        // TODO: log error
      }
      return nullptr;
    }

    return std::unique_ptr<Channel>(new SocketChannel(sock_fd));
  }

  std::future<Channel::Ptr> Accept(EventLoop &loop, std::function<void(rnetlib::Channel &)> on_established) override {
    on_established_ = std::move(on_established);

    // set socket to non-blocking mode
    SetNonBlocking(true);

    loop.AddHandler(*this);

    return promise_.get_future();
  }

  uint16_t GetListenPort() const override {
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    std::memset(&addr, 0, len);

    if (S_GETSOCKNAME(sock_fd_, reinterpret_cast<struct sockaddr *>(&addr), &len)) {
      return 0;
    }

    return ntohs(addr.sin_port);
  }

  int OnEvent(int event_type, void *arg) override {
    if (event_type & POLLIN) {
      auto channel = Accept();
      if (on_established_) {
        on_established_(*channel);
      }
      promise_.set_value(std::move(channel));
    }

    return MAY_BE_REMOVED;
  }

  int OnError(int error_type) override {
    // TODO: log error
    promise_.set_value(nullptr);
    return MAY_BE_REMOVED;
  }

  void *GetHandlerID() const override {
    return const_cast<int *>(&sock_fd_);
  }

  short GetEventType() const override {
    return POLLIN;
  }

 private:
  std::string bind_addr_;
  uint16_t bind_port_;
  std::promise<Channel::Ptr> promise_;
  std::function<void(rnetlib::Channel &)> on_established_;
};

} // namespace socket
} // namespace rnetlib

#endif // RNETLIB_SOCKET_SOCKET_SERVER_H
