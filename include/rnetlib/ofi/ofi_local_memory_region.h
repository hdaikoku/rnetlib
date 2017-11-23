#ifndef RNETLIB_OFI_OFI_LOCAL_MEMORY_REGION_H_
#define RNETLIB_OFI_OFI_LOCAL_MEMORY_REGION_H_

#include <rdma/fi_domain.h>

#include "rnetlib/local_memory_region.h"

namespace rnetlib {
namespace ofi {

class OFILocalMemoryRegion : public LocalMemoryRegion {
 public:
  OFILocalMemoryRegion(fid_mr *mr, void *addr, size_t len) : mr_(mr), addr_(addr), len_(len) {}

  ~OFILocalMemoryRegion() override {
    if (mr_) {
      fi_close(reinterpret_cast<struct fid *>(mr_));
    }
  }

  void *GetAddr() const override { return addr_; }

  size_t GetLength() const override { return len_; }

  void *GetLKey() const override { return mr_ ? fi_mr_desc(mr_) : nullptr; }

  uint64_t GetRKey() const override { return fi_mr_key(mr_); }

 private:
  struct fid_mr *mr_;
  void *addr_;
  size_t len_;
};

} // namespace ofi
} // namespace rnetlib

#endif // RNETLIB_OFI_OFI_LOCAL_MEMORY_REGION_H_
