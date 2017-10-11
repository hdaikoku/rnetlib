#ifndef RNETLIB_RDMA_RDMA_CLIENT_H_
#define RNETLIB_RDMA_RDMA_CLIENT_H_

#include "rnetlib/client.h"
#include "rnetlib/rdma/rdma_channel.h"

namespace rnetlib {
namespace rdma {

class RDMAClient : public Client, public EventHandler {
 public:
  RDMAClient(const std::string &peer_addr, uint16_t peer_port)
      : peer_addr_(peer_addr), peer_port_(peer_port) {}

  virtual ~RDMAClient() = default;

  Channel::Ptr Connect() override {
    auto id = RDMACommon::NewRDMACommID(peer_addr_.c_str(), peer_port_, 0);
    if (!id) {
      return nullptr;
    }

    channel_.reset(new RDMAChannel(std::move(id)));

    if (rdma_connect(const_cast<struct rdma_cm_id *>(channel_->GetIDPtr()), nullptr)) {
      return nullptr;
    }

    return Channel::Ptr(channel_.release());
  }

  std::future<Channel::Ptr> Connect(EventLoop &loop, std::function<void(Channel &)> on_established) override {
    on_established_ = std::move(on_established);

    auto id = RDMACommon::NewRDMACommID(peer_addr_.c_str(), peer_port_, 0);
    if (id) {
      channel_.reset(new RDMAChannel(std::move(id)));

      // migrate rdma_cm_id to the event loop
      loop.AddHandler(*this);

      if (rdma_connect(const_cast<struct rdma_cm_id *>(channel_->GetIDPtr()), nullptr)) {
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
      if (on_established_) {
        on_established_(*channel_);
      }
      promise_.set_value(Channel::Ptr(channel_.release()));
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
    return const_cast<struct rdma_cm_id *>(channel_->GetIDPtr());
  }

  short GetEventType() const override {
    return 0;
  }

 private:
  std::unique_ptr<RDMAChannel> channel_;
  std::string peer_addr_;
  uint16_t peer_port_;
  std::promise<Channel::Ptr> promise_;
  std::function<void(Channel &)> on_established_;
};

} // namespace rdma
} // namespace rnetlib

#endif // RNETLIB_RDMA_RDMA_CLIENT_H_
