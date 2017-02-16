//
// Created by Harunobu Daikoku on 2017/02/10.
//

#ifndef RNETLIB_SOCKET_SOCKET_CLIENT_H
#define RNETLIB_SOCKET_SOCKET_CLIENT_H

#include <cerrno>
#include <chrono>
#include <poll.h>
#include <thread>

#include "rnetlib/client.h"
#include "rnetlib/socket/socket_channel.h"

#ifdef USE_RDMA
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
#endif //USE_RDMA

namespace rnetlib {
namespace socket {
class SocketClient : public Client, public SocketCommon {
 public:

  SocketClient(const std::string &peer_addr, uint16_t peer_port)
      : peer_addr_(peer_addr), peer_port_(peer_port) {}

  virtual ~SocketClient() {}

  Channel::Ptr Connect() override {
    // TODO: timeout and backoff should be user-configurable
    auto timeout = std::chrono::minutes(3);
    int backoff_msecs = 1;

    auto start = std::chrono::steady_clock::now();
    while (true) {
      if (start + timeout < std::chrono::steady_clock::now()) {
        // timed-out
        // TODO: log error
        return nullptr;
      }

      auto addr_info = Init(peer_addr_.c_str(), peer_port_, 0);
      if (!addr_info) {
        // TODO: log error
        return nullptr;
      }

      auto ret = S_CONNECT(sock_fd_, S_DST_ADDR(addr_info), S_DST_ADDRLEN(addr_info));
      if (ret == -1) {
        Close();
        if (errno == ECONNREFUSED) {
          // the server is not ready yet.
          std::this_thread::sleep_for(std::chrono::milliseconds(backoff_msecs));
          backoff_msecs = std::min(1000, backoff_msecs * 2);
          continue;
        } else {
          // unrecoverable error.
          // TODO: log error
          return nullptr;
        }
      } else {
        // successfully connected.
        return std::unique_ptr<Channel>(new SocketChannel(sock_fd_));
      }
    }
  }

  std::future<Channel::Ptr> Connect(EventLoop &loop) override {
    auto addr_info = Init(peer_addr_.c_str(), peer_port_, 0);
    if (!addr_info) {
      // TODO: log error
      std::promise<Channel::Ptr> promise;
      promise.set_value(nullptr);
      return promise.get_future();
    }

    // set socket to non-blocking mode
    SetNonBlocking(true);

    if (S_CONNECT(sock_fd_, S_DST_ADDR(addr_info), S_DST_ADDRLEN(addr_info)) == 0) {
      // this may happen when connection got established immediately.
      std::promise<Channel::Ptr> promise;
      promise.set_value(std::unique_ptr<Channel>(new SocketChannel(sock_fd_)));
      return promise.get_future();
    } else {
      if (errno != EINPROGRESS) {
        // unrecoverable error.
        // TODO: log error
        Close();
        std::promise<Channel::Ptr> promise;
        promise.set_value(nullptr);
        return promise.get_future();
      }
    }

    std::packaged_task<Channel::Ptr()> task([&]() {
      // check if connection has been established
      int val = 0;
      GetSockOpt(SOL_SOCKET, SO_ERROR, val);

      switch (val) {
        case 0:
          // no errors observed
          break;
        case ECONNREFUSED:
          // the peer is not ready yet.
          // close socket, throw an exception and let the user try again.
          Close();
          throw ECONNREFUSED;
        default:
          // unrecoverable error.
          Close();
          return std::unique_ptr<Channel>(nullptr);
      }
      // connection has been established.
      return std::unique_ptr<Channel>(new SocketChannel(sock_fd_));
    });
    auto f = task.get_future();

    loop.AddHandler(std::unique_ptr<EventHandler>(new ConnectHandler(std::move(task), sock_fd_)));

    return f;
  }

 private:
  std::string peer_addr_;
  uint16_t peer_port_;

  class ConnectHandler : public EventHandler {
   public:

    ConnectHandler(std::packaged_task<Channel::Ptr()> task, int sock_fd) : task_(std::move(task)), sock_fd_(sock_fd) {}

    int OnEvent(int event_type) override {
      if (event_type & POLLOUT) {
        task_();
      }

      return MAY_BE_REMOVED;
    }

    int OnError(int error_type) override {
      if (error_type & (POLLHUP | POLLERR)) {
        task_();
      }

      return MAY_BE_REMOVED;
    }

    int GetHandlerID() const override {
      return sock_fd_;
    }

    short GetEventType() const override {
      return POLLOUT;
    }

   private:
    int sock_fd_;
    std::packaged_task<Channel::Ptr()> task_;

  };

};
}
}

#endif //RNETLIB_SOCKET_SOCKET_CLIENT_H
