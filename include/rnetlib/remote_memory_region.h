#ifndef RNETLIB_REMOTE_MEMORY_REGION_H_
#define RNETLIB_REMOTE_MEMORY_REGION_H_

#include "rnetlib/local_memory_region.h"

namespace rnetlib {

struct RemoteMemoryRegion {
  RemoteMemoryRegion() = default;
  explicit RemoteMemoryRegion(const LocalMemoryRegion &lmr)
      : addr(reinterpret_cast<uintptr_t>(lmr.GetAddr())), rkey(lmr.GetRKey()), length(lmr.GetLength()) {}

  uintptr_t addr;
  uint64_t rkey;
  size_t length;
};

} // namespace rnetlib

#endif // RNETLIB_REMOTE_MEMORY_REGION_H_
