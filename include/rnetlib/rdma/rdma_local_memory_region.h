//
// Created by Harunobu Daikoku on 2017/02/14.
//

#ifndef RNETLIB_RDMA_RDMA_LOCAL_MEMORY_REGION_H
#define RNETLIB_RDMA_RDMA_LOCAL_MEMORY_REGION_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <infiniband/verbs.h>

#include "rnetlib/local_memory_region.h"

namespace rnetlib {
namespace rdma {
class RDMALocalMemoryRegion : public LocalMemoryRegion {
 public:

  static std::unique_ptr<LocalMemoryRegion> Register(struct ibv_pd *pd, void *addr, size_t length, int type) {
    int ibv_mr_type = 0;

    if (type & MR_LOCAL_WRITE) {
      ibv_mr_type |= IBV_ACCESS_LOCAL_WRITE;
    }
    if (type & MR_REMOTE_WRITE) {
      ibv_mr_type |= IBV_ACCESS_REMOTE_WRITE;
      // If IBV_ACCESS_REMOTE_WRITE or IBV_ACCESS_REMOTE_ATOMIC is set,
      // then IBV_ACCESS_LOCAL_WRITE must be set too.
      ibv_mr_type |= IBV_ACCESS_LOCAL_WRITE;
    }
    if (type & MR_REMOTE_READ) {
      ibv_mr_type |= IBV_ACCESS_REMOTE_READ;
    }

    auto mr = ibv_reg_mr(pd, addr, length, ibv_mr_type);
    if (!mr) {
      // failed to registere MR
      return nullptr;
    }
    return std::unique_ptr<LocalMemoryRegion>(new RDMALocalMemoryRegion(mr));
  }

  virtual ~RDMALocalMemoryRegion() {
    if (mr_) {
      ibv_dereg_mr(mr_);
    }
  }

  void *GetAddr() const override {
    return mr_->addr;
  }

  size_t GetLength() const override {
    return mr_->length;
  }

  uint32_t GetLKey() const override {
    return mr_->lkey;
  }

  uint32_t GetRKey() const override {
    return mr_->rkey;
  }

 private:
  struct ibv_mr *mr_;

  RDMALocalMemoryRegion(struct ibv_mr *mr) : mr_(mr) {}

};
}
}

#endif //RNETLIB_RDMA_RDMA_LOCAL_MEMORY_REGION_H
