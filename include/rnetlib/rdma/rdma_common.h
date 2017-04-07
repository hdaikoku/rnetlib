//
// Created by Harunobu Daikoku on 2017/02/10.
//

#ifndef RNETLIB_RDMA_RDMA_COMMON_H
#define RNETLIB_RDMA_RDMA_COMMON_H

#include <memory>
#include <string>
#include <rdma/rdma_cma.h>

namespace rnetlib {
namespace rdma {
class RDMACommon {
 public:

  class RDMACMIDDeleter {
   public:

    void operator()(struct rdma_cm_id *id) const {
      rdma_destroy_ep(id);
    }

  };

  using RDMACommID = std::unique_ptr<struct rdma_cm_id, RDMACMIDDeleter>;

  static void SetInitAttr(struct ibv_qp_init_attr &init_attr) {
    std::memset(&init_attr, 0, sizeof(init_attr));
    init_attr.qp_type = IBV_QPT_RC;
    init_attr.cap.max_send_sge = init_attr.cap.max_recv_sge = 1;
    init_attr.cap.max_send_wr = 1;
    // to make Receive WR constantly available in QP,
    // max_recv_wr must be bigger than 2
    init_attr.cap.max_recv_wr = 2;
    init_attr.sq_sig_all = 1;
    // TODO: max_inline_data should be user-configurable
    init_attr.cap.max_inline_data = 16;
  }

  static RDMACommID NewRDMACommID(const char *addr, uint16_t port, int flags) {
    struct rdma_addrinfo hints, *tmp_addrinfo;

    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_qp_type = IBV_QPT_RC;
    hints.ai_flags = flags | RAI_FAMILY;
    hints.ai_port_space = RDMA_PS_TCP;

    int error = rdma_getaddrinfo(const_cast<char *>(addr),
                                 const_cast<char *>(std::to_string(port).c_str()),
                                 &hints,
                                 &tmp_addrinfo);
    if (error) {
      // TODO: log error
      return nullptr;
    }

    struct ibv_qp_init_attr init_attr;
    SetInitAttr(init_attr);

    struct rdma_cm_id *tmp_id;
    auto ret = rdma_create_ep(&tmp_id, tmp_addrinfo, nullptr, &init_attr);
    rdma_freeaddrinfo(tmp_addrinfo);
    if (ret) {
      // TODO: log error
      return nullptr;
    }

    return RDMACommID(tmp_id);
  }

};
}
}

#endif //RNETLIB_RDMA_RDMA_COMMON_H
