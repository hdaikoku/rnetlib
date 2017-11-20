#ifndef RNETLIB_SOCKET_SOCKET_LOCAL_MEMORY_REGION_H_
#define RNETLIB_SOCKET_SOCKET_LOCAL_MEMORY_REGION_H_

#include <cstddef>
#include <cstdint>

#include "rnetlib/local_memory_region.h"

namespace rnetlib {
namespace socket {

class SocketLocalMemoryRegion : public LocalMemoryRegion {
 public:
  SocketLocalMemoryRegion(void *addr, size_t length) : addr_(addr), length_(length) {}

  void *GetAddr() const override { return addr_; }

  size_t GetLength() const override { return length_; }

  void *GetLKey() const override { return nullptr; }

  uint64_t GetRKey() const override { return 0; }

 private:
  void *addr_;
  size_t length_;
};

} // namespace socket
} // namespace rnetlib

#endif // RNETLIB_SOCKET_SOCKET_LOCAL_MEMORY_REGION_H_
