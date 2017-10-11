#ifndef RNETLIB_VERBS_VERBS_SERVER_H_
#define RNETLIB_VERBS_VERBS_SERVER_H_

#include <cerrno>

#include "rnetlib/server.h"
#include "rnetlib/verbs/verbs_channel.h"

namespace rnetlib {
namespace verbs {

class VerbsServer : public Server, public EventHandler {
 public:
  VerbsServer(const std::string &bind_addr, uint16_t bind_port)
      : bind_addr_(bind_addr), bind_port_(bind_port) {}

  virtual ~VerbsServer() = default;

  bool Listen() override {
    listen_id_ = VerbsCommon::NewRDMACommID(bind_addr_.c_str(), bind_port_, RAI_PASSIVE);
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

    channel_.reset(new VerbsChannel(VerbsCommon::RDMACommID(new_id)));
    if (rdma_accept(const_cast<struct rdma_cm_id *>(channel_->GetIDPtr()), nullptr)) {
      return nullptr;
    }

    // FIXME: might be better to wait for RDMA_CM_EVENT_ESTABLISHED event
    return Channel::Ptr(channel_.release());
  }

  std::future<Channel::Ptr> Accept(EventLoop &loop, std::function<void(Channel &)> on_established) override {
    on_established_ = std::move(on_established);

    // migrate rdma_cm_id to event
    loop.AddHandler(*this);

    return promise_.get_future();
  }

  uint16_t GetListenPort() const override {
    return ntohs(rdma_get_src_port(listen_id_.get()));
  }

  int OnEvent(int event_type, void *arg) override {
    if (event_type == RDMA_CM_EVENT_CONNECT_REQUEST) {
      // got a connect request.
      // issue accept and wait for establishment.
      channel_.reset(new VerbsChannel(VerbsCommon::RDMACommID(reinterpret_cast<struct rdma_cm_id *>(arg))));
      auto id = channel_->GetIDPtr();

      struct ibv_qp_init_attr init_attr;
      VerbsCommon::SetInitAttr(init_attr);
      std::memset(&init_attr, 0, sizeof(init_attr));

      if (rdma_create_qp(const_cast<struct rdma_cm_id *>(id), id->pd, &init_attr)) {
        promise_.set_value(nullptr);
        return MAY_BE_REMOVED;
      }

      if (rdma_accept(const_cast<struct rdma_cm_id *>(id), nullptr)) {
        // TODO: log error
        promise_.set_value(nullptr);
      } else {
        // successfully issued accept
        return 0;
      }
    } else if (event_type == RDMA_CM_EVENT_ESTABLISHED) {
      // connection established.
      if (on_established_) {
        on_established_(*channel_);
      }
      promise_.set_value(Channel::Ptr(channel_.release()));
    }

    return MAY_BE_REMOVED;
  }

  int OnError(int error_type) override {
    promise_.set_value(nullptr);
    return MAY_BE_REMOVED;
  }

  void *GetHandlerID() const override {
    if (channel_) {
      return const_cast<struct rdma_cm_id *>(channel_->GetIDPtr());
    }

    return listen_id_.get();
  }

  short GetEventType() const override {
    return 0;
  }

 private:
  VerbsCommon::RDMACommID listen_id_;
  std::unique_ptr<VerbsChannel> channel_;
  std::string bind_addr_;
  uint16_t bind_port_;
  std::promise<Channel::Ptr> promise_;
  std::function<void(Channel &)> on_established_;
};

} // namespace verbs
} // namespace rnetlib

#endif // RNETLIB_VERBS_VERBS_SERVER_H_
