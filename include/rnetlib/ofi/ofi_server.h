#ifndef RNETLIB_OFI_OFI_SERVER_H_
#define RNETLIB_OFI_OFI_SERVER_H_

#include "rnetlib/server.h"
#include "rnetlib/ofi/ofi_endpoint.h"

namespace rnetlib {
namespace ofi {

class OFIServer : public Server {
 public:
  OFIServer(const std::string &bind_addr, uint16_t bind_port)
      : ep_(OFIEndpoint::GetInstance(bind_addr, bind_port, FI_SOURCE)), bind_port_(ep_.GetBindPort()) {}

  bool Listen() override {
    // Initial receive will get remote address
    srv_channel_.reset(new OFIChannel(ep_, FI_ADDR_UNSPEC, TAG_CTR));
    return true;
  }

  Channel::ptr Accept() override {
    struct ofi_addrinfo peer_info;
    srv_channel_->Recv(&peer_info, sizeof(peer_info));

    fi_addr_t peer_addr = FI_ADDR_UNSPEC;
    ep_.InsertAddr(&peer_info.addr, 1, &peer_addr);
    // initialize channel before sending a reply
    std::unique_ptr<OFIChannel> ch(new OFIChannel(ep_, peer_addr, TAG_MSG));
    ch->SetTag(TAG_CTR);
    ch->Send(&peer_info.addrlen, sizeof(peer_info.addrlen));
    ch->SetTag(TAG_MSG);

    return std::move(ch);
  }

  std::future<Channel::ptr> Accept(EventLoop &loop, std::function<void(Channel & )> on_established) override {
    std::promise<Channel::ptr> promise_;
    return promise_.get_future();
  }

  uint16_t GetListenPort() const override {
    return bind_port_;
  }

 private:
  OFIEndpoint &ep_;
  std::unique_ptr<OFIChannel> srv_channel_;
  uint16_t bind_port_;
};

} // namespace ofi
} // namespace rnetlib

#endif // RNETLIB_OFI_OFI_SERVER_H_
