//
// Created by Harunobu Daikoku on 2017/02/10.
//

#ifndef RNETLIB_RDMA_RDMA_CLIENT_H
#define RNETLIB_RDMA_RDMA_CLIENT_H

#include "rnetlib/client.h"
#include "rnetlib/rdma/rdma_channel.h"

namespace rnetlib {
namespace rdma {
class RDMAClient : public Client, public RDMACommon, public EventHandler {
 public:

  RDMAClient(const std::string &peer_addr, uint16_t peer_port)
      : peer_addr_(peer_addr), peer_port_(peer_port) {}

  virtual ~RDMAClient() {}

  Channel::Ptr Connect() override {
    if (!Open(peer_addr_.c_str(), peer_port_, 0)) {
      return nullptr;
    }

    if (rdma_connect(id_.get(), nullptr)) {
      return nullptr;
    }

    return std::unique_ptr<Channel>(new RDMAChannel(id_.release()));
  }

  std::future<Channel::Ptr> Connect(EventLoop &loop) override {
    if (!Open(peer_addr_.c_str(), peer_port_, 0)) {
      promise_.set_value(nullptr);
      return promise_.get_future();
    }

    // migrate rdma_cm_id to the event loop
    loop.AddHandler(*this);

    if (rdma_connect(id_.get(), nullptr)) {
      // TODO: handle error
      promise_.set_value(nullptr);
      return promise_.get_future();
    }

    return promise_.get_future();
  }

  int OnEvent(int event_type, void *arg) override {
    if (event_type == RDMA_CM_EVENT_ESTABLISHED) {
      promise_.set_value(std::unique_ptr<Channel>(new RDMAChannel(id_.release())));
    }

    return MAY_BE_REMOVED;
  }

  int OnError(int error_type) override {
    // TODO: throw error
    // promise_.set_exception(ECONNREFUSED);
    return MAY_BE_REMOVED;
  }

  void *GetHandlerID() const override {
    return id_.get();
  }

  short GetEventType() const override {
    return 0;
  }

 private:
  std::string peer_addr_;
  uint16_t peer_port_;
  std::promise<Channel::Ptr> promise_;

};
}
}

#endif //RNETLIB_RDMA_RDMA_CLIENT_H
