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

  std::future<Channel::Ptr> Connect(EventLoop &loop, std::function<void(Channel &)> on_established) override {
    on_established_ = std::move(on_established);

    if (Open(peer_addr_.c_str(), peer_port_, 0)) {
      // migrate rdma_cm_id to the event loop
      loop.AddHandler(*this);

      if (rdma_connect(id_.get(), nullptr)) {
        // TODO: handle error
        promise_.set_value(nullptr);
      }
    } else {
      // error
      promise_.set_value(nullptr);
    }

    return promise_.get_future();
  }

  int OnEvent(int event_type, void *arg) override {
    if (event_type == RDMA_CM_EVENT_ESTABLISHED) {
      std::unique_ptr<Channel> channel(new RDMAChannel(id_.release()));
      if (on_established_) {
        on_established_(*channel);
      }
      promise_.set_value(std::move(channel));
    }

    return MAY_BE_REMOVED;
  }

  int OnError(int error_type) override {
    if (error_type == RDMA_CM_EVENT_REJECTED) {
      // server is not ready yet.

    }
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
  std::function<void(Channel &)> on_established_;

};
}
}

#endif //RNETLIB_RDMA_RDMA_CLIENT_H
