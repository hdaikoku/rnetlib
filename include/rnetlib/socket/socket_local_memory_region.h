//
// Created by Harunobu Daikoku on 2017/02/14.
//

#ifndef RNETLIB_SOCKET_SOCKET_LOCAL_MEMORY_REGION_H
#define RNETLIB_SOCKET_SOCKET_LOCAL_MEMORY_REGION_H

#include <cstddef>
#include <cstdint>

#include "rnetlib/local_memory_region.h"

namespace rnetlib {
namespace socket {
class SocketLocalMemoryRegion : public LocalMemoryRegion {
 public:

  SocketLocalMemoryRegion(void *addr, size_t length) : addr_(addr), length_(length) {}

  void *GetAddr() const override {
    return addr_;
  }

  size_t GetLength() const override {
    return length_;
  }

  uint32_t GetLKey() const override {
    return 0;
  }

  uint32_t GetRKey() const override {
    return 0;
  }

 private:
  void *addr_;
  size_t length_;

};
}
}

#endif //RNETLIB_SOCKET_SOCKET_LOCAL_MEMORY_REGION_H
