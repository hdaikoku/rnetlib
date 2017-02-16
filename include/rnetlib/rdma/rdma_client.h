//
// Created by Harunobu Daikoku on 2017/02/10.
//

#ifndef RNETLIB_RDMA_RDMA_CLIENT_H
#define RNETLIB_RDMA_RDMA_CLIENT_H

#include "rnetlib/client.h"
#include "rnetlib/rdma/rdma_channel.h"

namespace rnetlib {
namespace rdma {
class RDMAClient : public Client, public RDMACommon {
 public:

  RDMAClient(const std::string &peer_addr, uint16_t peer_port)
      : peer_addr_(peer_addr), peer_port_(peer_port) {}

  virtual ~RDMAClient() {}

  Channel::Ptr Connect() override {
    if (!Init(peer_addr_.c_str(), peer_port_, 0)) {
      return nullptr;
    }

    if (rdma_connect(id_.get(), nullptr)) {
      return nullptr;
    }

    return std::unique_ptr<Channel>(new RDMAChannel(id_.release()));
  }

 private:
  std::string peer_addr_;
  uint16_t peer_port_;

};
}
}

#endif //RNETLIB_RDMA_RDMA_CLIENT_H
