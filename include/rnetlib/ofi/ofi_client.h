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
  explicit OFIClient(uint64_t self_desc) : ep_(OFIEndpoint::GetInstance(nullptr, nullptr)), self_desc_(self_desc) {
    std::memset(&tx_req_, 0, sizeof(tx_req_));
  }

  ~OFIClient() override = default;

  Channel::ptr Connect(const std::string &peer_addr, uint16_t peer_port, uint64_t peer_desc) override {
    fi_addr_t fi_addr = FI_ADDR_UNSPEC;
    ep_.InsertAddr(peer_addr.c_str(), peer_port, &fi_addr);

    auto &self_addrinfo = ep_.GetBindAddrInfo();
    self_addrinfo.desc = self_desc_;
    self_addrinfo.src_tag = ep_.GetNewSrcTag();
    auto lmr = ep_.RegisterMemoryRegion(&self_addrinfo, sizeof(self_addrinfo), MR_LOCAL_READ);
    std::unique_ptr<OFIChannel> ch(new OFIChannel(ep_, fi_addr, peer_desc, self_addrinfo.src_tag));

    ep_.PostSend(lmr->GetAddr(), lmr->GetLength(), lmr->GetLKey(), fi_addr,
                 (TAG_PROTO_CTR << OFI_TAG_SOURCE_BITS), &tx_req_);
    ep_.PollTxCQ(tx_req_.req, &tx_req_);
    assert(tx_req_.req == 0);

    uint64_t dst_tag = 0;
    ch->Recv(&dst_tag, sizeof(dst_tag));
    ch->SetDestTag(dst_tag);

    return std::move(ch);
  }

 private:
  OFIEndpoint &ep_;
  uint64_t self_desc_;
  struct ofi_req tx_req_;
};

} // namespace ofi
} // namespace rnetlib

#endif // RNETLIB_OFI_OFI_CLIENT_H_
