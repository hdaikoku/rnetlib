//
// Created by Harunobu Daikoku on 2017/02/10.
//

#ifndef RNETLIB_RDMA_RDMA_SERVER_H
#define RNETLIB_RDMA_RDMA_SERVER_H

#include <cerrno>

#include "rnetlib/server.h"
#include "rnetlib/rdma/rdma_channel.h"

namespace rnetlib {
namespace rdma {
class RDMAServer : public Server, public RDMACommon {
 public:

  RDMAServer(const std::string &bind_addr, uint16_t bind_port)
      : bind_addr_(bind_addr), bind_port_(bind_port) {}

  bool Listen() override {
    if (!Init(bind_addr_.c_str(), bind_port_, RAI_PASSIVE)) {
      // error
      return false;
    }

    // rdma_cm_id prepared by rdma_create_ep is set to sync. mode.
    // if it is meant to be used in async. mode, use rdma_migrate_id with user's event_handler
    if (rdma_listen(id_.get(), 1024)) {
      return false;
    }

    return true;
  }

  Channel::Ptr Accept() override {
    struct rdma_cm_id *new_id;

    if (rdma_get_request(id_.get(), &new_id)) {
      return nullptr;
    }

    if (rdma_accept(new_id, nullptr)) {
      rdma_destroy_id(new_id);
      return nullptr;
    }

    return std::unique_ptr<Channel>(new RDMAChannel(new_id));
  }

 private:
  std::string bind_addr_;
  uint16_t bind_port_;

};
}
}

#endif //RNETLIB_RDMA_RDMA_SERVER_H
