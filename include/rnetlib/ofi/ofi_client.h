#ifndef RNETLIB_OFI_OFI_CLIENT_H_
#define RNETLIB_OFI_OFI_CLIENT_H_

#include <cassert>

#include "rnetlib/client.h"
#include "rnetlib/ofi/ofi_channel.h"
#include "rnetlib/ofi/ofi_endpoint.h"

namespace rnetlib {
namespace ofi {

class OFIClient : public Client {
 public:
  OFIClient(const std::string &peer_addr, uint16_t peer_port, uint64_t self_desc, uint64_t peer_desc)
      : ep_(OFIEndpoint::GetInstance(peer_addr, peer_port, 0)),
        peer_addr_(peer_addr), self_desc_(self_desc), peer_desc_(peer_desc) { ep_.RegisterContext(&tx_ctx_); }

  ~OFIClient() override { ep_.DeregisterContext(&tx_ctx_); }

  Channel::ptr Connect() override {
    fi_addr_t peer_addr = FI_ADDR_UNSPEC;
    if (ep_.GetDestAddrPtr()) {
      ep_.InsertAddr(ep_.GetDestAddrPtr(), 1, &peer_addr);
    } else {
      ep_.InsertAddr(peer_addr_.c_str(), 1, &peer_addr);
    }

    auto &self_addrinfo = ep_.GetBindAddrInfo();
    self_addrinfo.desc = self_desc_;
    auto lmr = ep_.RegisterMemoryRegion(&self_addrinfo, sizeof(self_addrinfo), MR_LOCAL_READ);
    Channel::ptr ch(new OFIChannel(ep_, peer_addr, peer_desc_));

    ep_.PostSend(lmr->GetAddr(), lmr->GetLength(), lmr->GetLKey(), peer_addr, TAG_CTR, &tx_ctx_);
    ep_.PollTxCQ(OFI_CTX_WR(&tx_ctx_), &tx_ctx_);
    assert(OFI_CTX_WR(&tx_ctx_) == 0);

    ch->Recv(&self_addrinfo.addrlen, sizeof(self_addrinfo.addrlen));

    return std::move(ch);
  }

  std::future<Channel::ptr> Connect(EventLoop &loop, std::function<void(Channel &)> on_established) override {
    std::promise<Channel::ptr> pr;
    return pr.get_future();
  }

 private:
  OFIEndpoint &ep_;
  std::string peer_addr_;
  uint64_t self_desc_;
  uint64_t peer_desc_;
  struct ofi_context tx_ctx_;
};

} // namespace ofi
} // namespace rnetlib

#endif // RNETLIB_OFI_OFI_CLIENT_H_
