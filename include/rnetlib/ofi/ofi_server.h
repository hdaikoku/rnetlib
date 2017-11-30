#ifndef RNETLIB_OFI_OFI_SERVER_H_
#define RNETLIB_OFI_OFI_SERVER_H_

#include <array>

#include "rnetlib/server.h"
#include "rnetlib/ofi/ofi_endpoint.h"

namespace rnetlib {
namespace ofi {

class OFIServer : public Server {
 public:
  OFIServer(const std::string &bind_addr, uint16_t bind_port)
      : ep_(OFIEndpoint::GetInstance(nullptr, std::to_string(bind_port).c_str())), ai_idx_(0) {
    // FIXME: bind endpoint to a specific local address
    ep_.RegisterContext(&rx_ctx_);
  }

  ~OFIServer() override { ep_.DeregisterContext(&rx_ctx_); }

  bool Listen() override {
    ai_lmrs_[0] = ep_.RegisterMemoryRegion(&peer_ai_[0], sizeof(peer_ai_[0]), MR_LOCAL_WRITE);
    ai_lmrs_[1] = ep_.RegisterMemoryRegion(&peer_ai_[1], sizeof(peer_ai_[1]), MR_LOCAL_WRITE);
    PostAccept();
    return true;
  }

  Channel::ptr Accept() override {
    PostAccept();
    ep_.PollRxCQ(OFI_CTX_WR(&rx_ctx_) - 1, &rx_ctx_);
    assert(OFI_CTX_WR(&rx_ctx_) == 1);
    
    fi_addr_t peer_addr = FI_ADDR_UNSPEC;
    ep_.InsertAddr(&peer_ai_[ai_idx_].addr, &peer_addr);

    Channel::ptr ch(new OFIChannel(ep_, peer_addr, peer_ai_[ai_idx_].desc));
    ch->Send(&peer_ai_[ai_idx_].addrlen, sizeof(peer_ai_[ai_idx_].addrlen));

    return std::move(ch);
  }

  std::future<Channel::ptr> Accept(EventLoop &loop, std::function<void(Channel & )> on_established) {
    std::promise<Channel::ptr> promise;
    return promise.get_future();
  }

  std::string GetRawAddr() const override {
    const auto &self_addrinfo = ep_.GetBindAddrInfo();
    return std::string(self_addrinfo.addr, self_addrinfo.addrlen);
  }

  uint16_t GetListenPort() const override { return 0; }

 private:
  OFIEndpoint &ep_;
  int ai_idx_;
  std::array<struct ofi_addrinfo, 2> peer_ai_;
  std::array<LocalMemoryRegion::ptr, 2> ai_lmrs_;
  struct ofi_context rx_ctx_;

  void PostAccept() {
    ep_.PostRecv(ai_lmrs_[ai_idx_]->GetAddr(), ai_lmrs_[ai_idx_]->GetLength(), ai_lmrs_[ai_idx_]->GetLKey(),
                 FI_ADDR_UNSPEC, TAG_CTR, &rx_ctx_);
    ai_idx_ = !ai_idx_;
  }
};

} // namespace ofi
} // namespace rnetlib

#endif // RNETLIB_OFI_OFI_SERVER_H_
