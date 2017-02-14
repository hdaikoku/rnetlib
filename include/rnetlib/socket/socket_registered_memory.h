//
// Created by Harunobu Daikoku on 2017/02/14.
//

#ifndef RNETLIB_SOCKET_SOCKET_REGISTERED_MEMORY_H
#define RNETLIB_SOCKET_SOCKET_REGISTERED_MEMORY_H

#include <cstddef>
#include <cstdint>
#include "rnetlib/registered_memory.h"

namespace rnetlib {
namespace socket {
class SocketRegisteredMemory : public RegisteredMemory {
 public:

  SocketRegisteredMemory(void *addr, size_t length) : addr_(addr), length_(length) {}

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

#endif //RNETLIB_SOCKET_SOCKET_REGISTERED_MEMORY_H
