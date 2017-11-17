#ifndef RNETLIB_LOCAL_MEMORY_REGION_H_
#define RNETLIB_LOCAL_MEMORY_REGION_H_

#include <memory>

enum MRType {
  MR_LOCAL_READ   = 0,
  MR_LOCAL_WRITE  = (1 << 0),
  MR_REMOTE_READ  = (1 << 1),
  MR_REMOTE_WRITE = (1 << 2)
};

namespace rnetlib {

class LocalMemoryRegion {
 public:
  using ptr = std::unique_ptr<LocalMemoryRegion>;

  virtual ~LocalMemoryRegion() = default;

  virtual void *GetAddr() const = 0;

  virtual size_t GetLength() const = 0;

  virtual uint64_t GetLKey() const = 0;

  virtual uint64_t GetRKey() const = 0;
};

} // namespace rnetlib

#endif // RNETLIB_LOCAL_MEMORY_REGION_H_
