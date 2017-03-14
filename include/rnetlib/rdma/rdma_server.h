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

  std::future<Channel::Ptr> Accept(EventLoop &loop, std::function<void(Channel &)> on_established) override {
    on_established_ = std::move(on_established);

    // migrate rdma_cm_id to event
    loop.AddHandler(*this);

    if (rdma_listen(id_.get(), 1024)) {
      promise_.set_value(nullptr);
    }

    return promise_.get_future();
  }

  int OnEvent(int event_type, void *arg) override {
    if (event_type == RDMA_CM_EVENT_CONNECT_REQUEST) {
      // got a connect request.
      // issue accept and wait for establishment.
      accepting_id_.reset(reinterpret_cast<struct rdma_cm_id *>(arg));

      struct ibv_qp_init_attr init_attr;
      std::memset(&init_attr, 0, sizeof(init_attr));
      init_attr.qp_type = IBV_QPT_RC;
      init_attr.cap.max_send_sge = init_attr.cap.max_recv_sge = 1;
      init_attr.cap.max_send_wr = init_attr.cap.max_recv_wr = 1;
      init_attr.sq_sig_all = 1;
      // TODO: max_inline_data should be user-configurable
      init_attr.cap.max_inline_data = 16;

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
      std::unique_ptr<Channel> channel(new RDMAChannel(accepting_id_.release()));
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

    return id_.get();
  }

  short GetEventType() const override {
    return 0;
  }

 private:
  std::string bind_addr_;
  uint16_t bind_port_;
  std::promise<Channel::Ptr> promise_;
  std::function<void(Channel &)> on_established_;
  std::unique_ptr<struct rdma_cm_id, RDMACMIDDeleter> accepting_id_;

};
}
}

#endif //RNETLIB_RDMA_RDMA_SERVER_H
