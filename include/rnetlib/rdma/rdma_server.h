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
class RDMAServer : public Server, public EventHandler {
 public:

  RDMAServer(const std::string &bind_addr, uint16_t bind_port)
      : bind_addr_(bind_addr), bind_port_(bind_port) {}

  virtual ~RDMAServer() = default;

  bool Listen() override {
    // rdma_cm_id prepared by rdma_create_ep is set to sync. mode.
    // if it is meant to be used in async. mode, use rdma_migrate_id with user's event_handler

    listen_id_ = RDMACommon::NewRDMACommID(bind_addr_.c_str(), bind_port_, RAI_PASSIVE);
    if (!listen_id_) {
      // TODO: log error
      return false;
    }

    if (rdma_listen(listen_id_.get(), 1024)) {
      // TODO: log error
      return false;
    }

    return true;
  }

  Channel::Ptr Accept() override {
    struct rdma_cm_id *new_id;

    if (rdma_get_request(listen_id_.get(), &new_id)) {
      return nullptr;
    }

    if (rdma_accept(new_id, nullptr)) {
      rdma_destroy_id(new_id);
      return nullptr;
    }

    // FIXME: might be better to wait for RDMA_CM_EVENT_ESTABLISHED event
    return std::unique_ptr<Channel>(new RDMAChannel(RDMACommon::RDMACommID(new_id)));
  }

  std::future<Channel::Ptr> Accept(EventLoop &loop, std::function<void(Channel &)> on_established) override {
    on_established_ = std::move(on_established);

    // migrate rdma_cm_id to event
    loop.AddHandler(*this);

    return promise_.get_future();
  }

  int OnEvent(int event_type, void *arg) override {
    if (event_type == RDMA_CM_EVENT_CONNECT_REQUEST) {
      // got a connect request.
      // issue accept and wait for establishment.
      accepting_id_.reset(reinterpret_cast<struct rdma_cm_id *>(arg));

      struct ibv_qp_init_attr init_attr;
      RDMACommon::SetInitAttr(init_attr);
      std::memset(&init_attr, 0, sizeof(init_attr));

      if (rdma_create_qp(accepting_id_.get(), accepting_id_->pd, &init_attr)) {
        promise_.set_value(nullptr);
        return MAY_BE_REMOVED;
      }

      if (rdma_accept(accepting_id_.get(), nullptr)) {
        // TODO: log error
        promise_.set_value(nullptr);
      } else {
        // successfully issued accept
        return 0;
      }
    } else if (event_type == RDMA_CM_EVENT_ESTABLISHED) {
      // connection established.
      std::unique_ptr<Channel> channel(new RDMAChannel(std::move(accepting_id_)));
      if (on_established_) {
        on_established_(*channel);
      }
      promise_.set_value(std::move(channel));
    }

    return MAY_BE_REMOVED;
  }

  int OnError(int error_type) override {
    promise_.set_value(nullptr);
    return MAY_BE_REMOVED;
  }

  void *GetHandlerID() const override {
    if (accepting_id_) {
      return accepting_id_.get();
    }

    return listen_id_.get();
  }

  short GetEventType() const override {
    return 0;
  }

 private:
  RDMACommon::RDMACommID listen_id_;
  RDMACommon::RDMACommID accepting_id_;
  std::string bind_addr_;
  uint16_t bind_port_;
  std::promise<Channel::Ptr> promise_;
  std::function<void(Channel &)> on_established_;

};
}
}

#endif //RNETLIB_RDMA_RDMA_SERVER_H
