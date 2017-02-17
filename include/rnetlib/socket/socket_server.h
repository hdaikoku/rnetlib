//
// Created by Harunobu Daikoku on 2017/02/10.
//

#ifndef RNETLIB_SOCKET_SOCKET_SERVER_H
#define RNETLIB_SOCKET_SOCKET_SERVER_H

#include <cerrno>
#include <poll.h>

#include "rnetlib/event_handler.h"
#include "rnetlib/server.h"
#include "rnetlib/socket/socket_channel.h"

#ifdef USE_RDMA
// rsocket-specific functions
#define S_BIND(s, a, l)   rbind(s, a, l)
#define S_LISTEN(s, b)    rlisten(s, b)
#define S_ACCEPT(s, a, l) raccept(s, a, l)
#define S_SRC_ADDR(a)     a->ai_src_addr
#define S_SRC_ADDRLEN(a)  a->ai_src_len
#define S_PASSIVE         RAI_PASSIVE
#else
// BSD socket-specific functions
#define S_BIND(s, a, l)   bind(s, a, l)
#define S_LISTEN(s, b)    listen(s, b)
#define S_ACCEPT(s, a, l) accept(s, a, l)
#define S_SRC_ADDR(a)     a->ai_addr
#define S_SRC_ADDRLEN(a)  a->ai_addrlen
#define S_PASSIVE         AI_PASSIVE
#endif

namespace rnetlib {
namespace socket {
class SocketServer : public Server, public SocketCommon {
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

  std::future<Channel::Ptr> Accept(EventLoop &loop) override {
    // set socket to non-blocking mode
    SetNonBlocking(true);

    std::packaged_task<Channel::Ptr()> task([&]() {
      return Accept();
    });
    auto f = task.get_future();

    loop.AddHandler(std::unique_ptr<EventHandler>(new AcceptHandler(std::move(task), sock_fd_)));

    return f;
  }

 private:
  std::string bind_addr_;
  uint16_t bind_port_;

  class AcceptHandler : public EventHandler {
   public:
    AcceptHandler(std::packaged_task<Channel::Ptr()> task, int sock_fd) : task_(std::move(task)), sock_fd_(sock_fd) {}

    int OnEvent(int event_type) override {
      if (event_type & POLLIN) {
        task_();
      }

      return MAY_BE_REMOVED;
    }

    int OnError(int error_type) override {
      // TODO: log error
      return MAY_BE_REMOVED;
    }

    int GetHandlerID() const override {
      return sock_fd_;
    }

    short GetEventType() const override {
      return POLLIN;
    }

   private:
    int sock_fd_;
    std::packaged_task<Channel::Ptr()> task_;

  };

};
}
}

#endif //RNETLIB_SOCKET_SOCKET_SERVER_H
