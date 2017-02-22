//
// Created by Harunobu Daikoku on 2017/02/16.
//

#ifndef RNETLIB_SOCKET_SOCKET_EVENT_LOOP_H
#define RNETLIB_SOCKET_SOCKET_EVENT_LOOP_H

#include <poll.h>
#include <vector>

#include "rnetlib/event_loop.h"

#ifdef USE_RDMA
// rsocket-specific functions
#include <rdma/rsocket.h>
#define S_POLL(f, n, t)              rpoll(f, n, t)
#else
// BSD-socket specific functions
#define S_POLL(f, n, t)              poll(f, n, t)
#endif //USE_RDMA

namespace rnetlib {
namespace socket {
class SocketEventLoop : public EventLoop {
 public:

  void AddHandler(std::unique_ptr<EventHandler> handler) override {
    handlers_.push_back(std::move(handler));
  }

  int Run(int timeout) override {
    while (handlers_.size() > 0) {
      std::vector<struct pollfd> fds;
      for (const auto &handler : handlers_) {
        int sock_fd = *(reinterpret_cast<const int *>(handler->GetHandlerID()));
        fds.emplace_back(pollfd{sock_fd, handler->GetEventType(), 0});
      }
      nfds_t num_fds = static_cast<nfds_t>(fds.size());

      bool cleanup = false;
      auto rc = S_POLL(fds.data(), num_fds, timeout);
      if (rc < 0) {
        // TODO: log error
        return kErrFailed;
      } else if (rc == 0) {
        // TODO: log error
        return kErrTimedOut;
      }

      for (int i = 0; i < num_fds; i++) {
        auto &pfd = fds[i];
        auto &revents = pfd.revents;
        auto &handler = handlers_[i];

        if (revents == 0) {
          // this file descriptor is not ready yet
          continue;
        }

        if (revents & (POLLHUP | POLLERR | POLLNVAL)) {
          // connection has been closed
          if (handler->OnError(revents) == MAY_BE_REMOVED) {
            handler.reset();
            cleanup = true;
          }
        } else {
          if (handler->OnEvent(revents) == MAY_BE_REMOVED) {
            handler.reset();
            cleanup = true;
          }
        }

        revents = 0;
      }

      if (cleanup) {
        handlers_.erase(std::remove_if(handlers_.begin(), handlers_.end(),
                                       [](const std::unique_ptr<EventHandler> &handler) {
                                         return !handler;
                                       }),
                        handlers_.end());
      }
    }

    return 0;
  }

 private:
  std::vector<std::unique_ptr<EventHandler>> handlers_;

};
}
}

#endif //RNETLIB_SOCKET_SOCKET_EVENT_LOOP_H
