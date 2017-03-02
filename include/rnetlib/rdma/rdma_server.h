//
// Created by Harunobu Daikoku on 2017/02/10.
//

#ifndef RNETLIB_RDMA_RDMA_SERVER_H
#define RNETLIB_RDMA_RDMA_SERVER_H

#include <cerrno>

#include "rnetlib/server.h"
#include "rnetlib/rdma/rdma_channel.h"

namespace rnetlib {
namespace rdma {
class RDMAServer : public Server, public RDMACommon, public EventHandler {
 public:

  RDMAServer(const std::string &bind_addr, uint16_t bind_port)
      : bind_addr_(bind_addr), bind_port_(bind_port) {}

  bool Listen() override {
    // rdma_cm_id prepared by rdma_create_ep is set to sync. mode.
    // if it is meant to be used in async. mode, use rdma_migrate_id with user's event_handler

    return Open(bind_addr_.c_str(), bind_port_, RAI_PASSIVE);
  }

  Channel::Ptr Accept() override {
    struct rdma_cm_id *new_id;

    if (rdma_listen(id_.get(), 1024)) {
      return false;
    }

    if (rdma_get_request(id_.get(), &new_id)) {
      return nullptr;
    }

    if (rdma_accept(new_id, nullptr)) {
      rdma_destroy_id(new_id);
      return nullptr;
    }

    // FIXME: might be better to wait for RDMA_CM_EVENT_ESTABLISHED event
    return std::unique_ptr<Channel>(new RDMAChannel(new_id));
  }

  std::future<Channel::Ptr> Accept(EventLoop &loop) override {
    // migrate rdma_cm_id to event
    loop.AddHandler(*this);

    if (rdma_listen(id_.get(), 1024)) {
      promise_.set_value(nullptr);
    }

    return promise_.get_future();
  }

  int OnEvent(int event_type, void *arg) override {
    if (event_type == RDMA_CM_EVENT_CONNECT_REQUEST) {
      auto new_id = reinterpret_cast<struct rdma_cm_id *>(arg);
      // got a connect request.
      // issue accept and wait for establishment.
      if (rdma_accept(new_id, nullptr)) {
        rdma_destroy_id(new_id);
        promise_.set_value(nullptr);
      } else {
        promise_.set_value(std::unique_ptr<Channel>(new RDMAChannel(new_id)));
      }
    } else if (event_type == RDMA_CM_EVENT_ESTABLISHED) {
      // FIXME: might be better to wait for RDMA_CM_EVENT_ESTABLISHED event
    }

    return MAY_BE_REMOVED;
  }

  int OnError(int error_type) override {
    promise_.set_value(nullptr);
    return MAY_BE_REMOVED;
  }

  void *GetHandlerID() const override {
    return id_.get();
  }

  short GetEventType() const override {
    return 0;
  }

 private:
  std::string bind_addr_;
  uint16_t bind_port_;
  std::promise<Channel::Ptr> promise_;

};
}
}

#endif //RNETLIB_RDMA_RDMA_SERVER_H
