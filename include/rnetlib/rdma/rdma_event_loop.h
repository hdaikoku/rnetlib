//
// Created by Harunobu Daikoku on 2017/02/20.
//

#ifndef RNETLIB_RDMA_RDMA_EVENT_LOOP_H
#define RNETLIB_RDMA_RDMA_EVENT_LOOP_H

#include <algorithm>
#include <functional>
#include <memory>
#include <rdma/rdma_cma.h>
#include <vector>

#include "rnetlib/event_loop.h"

namespace rnetlib {
namespace rdma {
class RDMAEventLoop : public EventLoop {
 public:

  // deleter class for rdma_event_channel
  class RDMAEventChannelDeleter {
   public:
    void operator()(struct rdma_event_channel *ec) const {
      if (ec) {
        rdma_destroy_event_channel(ec);
      }
    }
  };

  RDMAEventLoop() : event_channel_(rdma_create_event_channel()) {}

  void AddHandler(EventHandler &handler) override {
    auto id = reinterpret_cast<struct rdma_cm_id *>(handler.GetHandlerID());
    rdma_migrate_id(id, event_channel_.get());
    handlers_.emplace_back(std::ref(handler));
  }

  int WaitAll(int timeout_millis) override {
    struct rdma_cm_event *ev;

    while (handlers_.size() > 0) {
      // get one event from the event channel
      if (rdma_get_cm_event(event_channel_.get(), &ev)) {
        // TODO: log error
        return kErrFailed;
      }

      auto handler_itr = std::find_if(handlers_.begin(), handlers_.end(),
                                      [&ev](const EventHandler &handler) {
                                        return (handler.GetHandlerID() == ev->id
                                            || handler.GetHandlerID() == ev->listen_id);
                                      });
      if (handler_itr == handlers_.end()) {
        // got an event on an unknown handler
        rdma_ack_cm_event(ev);
        return kErrFailed;
      }
      auto &handler = (*handler_itr).get();

      switch (ev->event) {
        case RDMA_CM_EVENT_CONNECT_REQUEST:
          // got a connect request on passive side.
          // connection got established successfully.
          if (handler.OnEvent(ev->event, ev->id) == MAY_BE_REMOVED) {
            handlers_.erase(handler_itr);
          }
          break;
        case RDMA_CM_EVENT_ESTABLISHED:
          // connection got established successfully.
          if (handler.OnEvent(ev->event, nullptr) == MAY_BE_REMOVED) {
            handlers_.erase(handler_itr);
          }
          break;
        case RDMA_CM_EVENT_CONNECT_ERROR:
        case RDMA_CM_EVENT_UNREACHABLE:
        case RDMA_CM_EVENT_REJECTED:
          // connection failed to be established.
          if (handler.OnError(ev->event) == MAY_BE_REMOVED) {
            handlers_.erase(handler_itr);
          }
          break;
        default:
          break;
      }

      rdma_ack_cm_event(ev);
    }

    return 0;
  }

 private:
  std::unique_ptr<struct rdma_event_channel, RDMAEventChannelDeleter> event_channel_;
  std::vector<std::reference_wrapper<EventHandler>> handlers_;

};
}
}

#endif //RNETLIB_RDMA_RDMA_EVENT_LOOP_H
