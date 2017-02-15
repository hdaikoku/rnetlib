//
// Created by Harunobu Daikoku on 2017/02/10.
//

#ifndef RNETLIB_CHANNEL_H
#define RNETLIB_CHANNEL_H

#include "rnetlib/registered_memory.h"

namespace rnetlib {
class Channel {
 public:

  virtual size_t Send(void *buf, size_t len) const = 0;

  virtual size_t Recv(void *buf, size_t len) const = 0;

  virtual size_t Send(RegisteredMemory &mem) const = 0;

  virtual size_t Recv(RegisteredMemory &mem) const = 0;

  virtual std::unique_ptr<RegisteredMemory> RegisterMemory(void *addr, size_t len, int type) const = 0;

};
}

#endif //RNETLIB_CHANNEL_H