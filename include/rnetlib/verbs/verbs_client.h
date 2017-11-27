#ifndef RNETLIB_VERBS_VERBS_CLIENT_H_
#define RNETLIB_VERBS_VERBS_CLIENT_H_

#include "rnetlib/client.h"
#include "rnetlib/verbs/verbs_channel.h"

namespace rnetlib {
namespace verbs {

class VerbsClient : public Client, public EventHandler {
 public:
  VerbsClient(const std::string &peer_addr, uint16_t peer_port, uint64_t self_desc, uint64_t peer_desc)
      : peer_addr_(peer_addr), peer_port_(peer_port), self_desc_(self_desc), peer_desc_(peer_desc) {}

  ~VerbsClient() = default;

  Channel::ptr Connect() override {
    auto id = VerbsCommon::NewRDMACommID(peer_addr_.c_str(), peer_port_, 0);
    if (!id) {
      return nullptr;
    }

    channel_.reset(new VerbsChannel(std::move(id), peer_desc_));

    struct rdma_conn_param conn_param;
    std::memset(&conn_param, 0, sizeof(conn_param));
    conn_param.private_data = &self_desc_;
    conn_param.private_data_len = sizeof(self_desc_);
    conn_param.responder_resources = RDMA_MAX_RESP_RES;
    conn_param.initiator_depth = RDMA_MAX_INIT_DEPTH;
    conn_param.flow_control = 1;
    conn_param.retry_count = 15;
    conn_param.rnr_retry_count = 7;

    if (rdma_connect(const_cast<struct rdma_cm_id *>(channel_->GetIDPtr()), &conn_param)) {
      return nullptr;
    }

    return std::move(channel_);
  }

  std::future<Channel::ptr> Connect(EventLoop &loop, std::function<void(Channel &)> on_established) override {
    on_established_ = std::move(on_established);

    auto id = VerbsCommon::NewRDMACommID(peer_addr_.c_str(), peer_port_, 0);
    if (id) {
      channel_.reset(new VerbsChannel(std::move(id)));

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
      promise_.set_value(Channel::ptr(channel_.release()));
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
  std::unique_ptr<VerbsChannel> channel_;
  std::string peer_addr_;
  uint16_t peer_port_;
  uint64_t self_desc_;
  uint64_t peer_desc_;
  std::promise<Channel::ptr> promise_;
  std::function<void(Channel &)> on_established_;
};

} // namespace verbs
} // namespace rnetlib

#endif // RNETLIB_VERBS_VERBS_CLIENT_H_
