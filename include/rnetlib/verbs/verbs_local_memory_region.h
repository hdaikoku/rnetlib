#ifndef RNETLIB_VERBS_VERBS_LOCAL_MEMORY_REGION_H_
#define RNETLIB_VERBS_VERBS_LOCAL_MEMORY_REGION_H_

#include <infiniband/verbs.h>

#include <cstddef>
#include <cstdint>
#include <memory>

#include "rnetlib/local_memory_region.h"

namespace rnetlib {
namespace verbs {

class VerbsLocalMemoryRegion : public LocalMemoryRegion {
 public:
  static std::unique_ptr<LocalMemoryRegion> Register(struct ibv_pd *pd, void *addr, size_t length, int type) {
    if (length == 0) {
      return std::unique_ptr<LocalMemoryRegion>(new VerbsLocalMemoryRegion(nullptr));
    }

    int ibv_mr_type = 0;
    if (type & MR_LOCAL_WRITE) {
      ibv_mr_type |= IBV_ACCESS_LOCAL_WRITE;
    }
    if (type & MR_REMOTE_READ) {
      ibv_mr_type |= IBV_ACCESS_REMOTE_READ;
    }
    if (type & MR_REMOTE_WRITE) {
      ibv_mr_type |= IBV_ACCESS_REMOTE_WRITE;
      // If IBV_ACCESS_REMOTE_WRITE or IBV_ACCESS_REMOTE_ATOMIC is set,
      // then IBV_ACCESS_LOCAL_WRITE must be set too.
      ibv_mr_type |= IBV_ACCESS_LOCAL_WRITE;
    }

    auto mr = ibv_reg_mr(pd, addr, length, ibv_mr_type);
    if (!mr) {
      // failed to registere MR
      return nullptr;
    }

    return std::unique_ptr<LocalMemoryRegion>(new VerbsLocalMemoryRegion(mr));
  }

  virtual ~VerbsLocalMemoryRegion() {
    if (mr_) {
      ibv_dereg_mr(mr_);
    }
  }

  void *GetAddr() const override { return mr_ ? mr_->addr : nullptr; }

  size_t GetLength() const override { return mr_ ? mr_->length : 0; }

  uint32_t GetLKey() const override { return mr_ ? mr_->lkey : 0; }

  uint32_t GetRKey() const override { return mr_ ? mr_->rkey : 0; }

 private:
  struct ibv_mr *mr_;

  explicit VerbsLocalMemoryRegion(struct ibv_mr *mr) : mr_(mr) {}
};

} // namespace verbs
} // namespace rnetlib

#endif // RNETLIB_VERBS_VERBS_LOCAL_MEMORY_REGION_H_
