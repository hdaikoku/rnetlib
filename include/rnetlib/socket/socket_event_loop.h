#ifndef RNETLIB_SOCKET_SOCKET_EVENT_LOOP_H_
#define RNETLIB_SOCKET_SOCKET_EVENT_LOOP_H_

#include <poll.h>

#include <algorithm>
#include <functional>
#include <unordered_map>
#include <vector>

#include "rnetlib/event_loop.h"

#ifdef RNETLIB_USE_RDMA
// rsocket-specific functions
#include <rdma/rsocket.h>
#define S_POLL(f, n, t)              rpoll(f, n, t)
#else
// BSD-socket specific functions
#define S_POLL(f, n, t)              poll(f, n, t)
#endif // RNETLIB_USE_RDMA

namespace rnetlib {
namespace socket {

class SocketEventLoop : public EventLoop {
 public:
  void AddHandler(EventHandler &handler) override {
    auto sock_fd = *(reinterpret_cast<int *>(handler.GetHandlerID()));
    if (handler_refs_.find(sock_fd) == handler_refs_.end()) {
      handler_refs_.emplace(std::make_pair(sock_fd, std::ref(handler)));
    }
  }

  int WaitAll(int timeout_millis) override {
    std::vector<struct pollfd> fds;

    while (!handler_refs_.empty()) {
      fds.clear();

      for (const auto &handler_ref : handler_refs_) {
        fds.emplace_back(pollfd{handler_ref.first, handler_ref.second.get().GetEventType(), 0});
      }
      nfds_t num_fds = static_cast<nfds_t>(fds.size());

      auto rc = S_POLL(fds.data(), num_fds, timeout_millis);
      if (rc < 0) {
        // miscellaneous errors
        // TODO: log error
        return kErrFailed;
      } else if (rc == 0) {
        // timed out
        // TODO: log error
        return kErrTimedOut;
      }

      for (int i = 0; i < num_fds; i++) {
        auto &pfd = fds[i];
        auto sock_fd = pfd.fd;
        auto &revents = pfd.revents;
        auto &handler = handler_refs_.at(sock_fd).get();

        if (revents == 0) {
          // this file descriptor is not ready yet
          continue;
        } else if (revents & (POLLHUP | POLLERR | POLLNVAL)) {
          // connection has been closed
          if (handler.OnError(revents) == MAY_BE_REMOVED) {
            handler_refs_.erase(sock_fd);
          }
        } else {
          // got an event
          if (handler.OnEvent(revents, nullptr) == MAY_BE_REMOVED) {
            handler_refs_.erase(sock_fd);
          }
        }

        revents = 0;
      }
    }

    return 0;
  }

 private:
  std::unordered_map<int, std::reference_wrapper<EventHandler>> handler_refs_;
};

} // namespace socket
} // namespace rnetlib

#endif // RNETLIB_SOCKET_SOCKET_EVENT_LOOP_H_
