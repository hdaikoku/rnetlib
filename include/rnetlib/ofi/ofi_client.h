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
  OFIClient(const std::string &peer_addr, uint16_t peer_port)
      : ep_(OFIEndpoint::GetInstance(peer_addr, peer_port, 0)) {}

  Channel::ptr Connect() override {
    fi_addr_t peer_addr = FI_ADDR_UNSPEC;
    assert(ep_.GetDestAddrPtr());
    ep_.InsertAddr(ep_.GetDestAddrPtr(), 1, &peer_addr);

    struct ofi_addrinfo self_info;
    ep_.GetAddrInfo(&self_info);

    std::unique_ptr<OFIChannel> ch(new OFIChannel(ep_, peer_addr, TAG_CTR));
    ch->Send(&self_info, sizeof(self_info));
    ch->SetTag(TAG_MSG);
    ch->Recv(&self_info.addrlen, sizeof(self_info.addrlen));

    return std::move(ch);
  }

  std::future<Channel::ptr> Connect(EventLoop &loop, std::function<void(Channel &)> on_established) override {
    std::promise<Channel::ptr> pr;
    return pr.get_future();
  }

 private:
  OFIEndpoint &ep_;
};

} // namespace ofi
} // namespace rnetlib

#endif // RNETLIB_OFI_OFI_CLIENT_H_
