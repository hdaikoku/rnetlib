//
// Created by Harunobu Daikoku on 2017/02/13.
//

#ifndef RNETLIB_REGISTERED_MEMORY_H
#define RNETLIB_REGISTERED_MEMORY_H

namespace rnetlib {
class RegisteredMemory {
 public:

  virtual void *GetAddr() const = 0;

  virtual size_t GetLength() const = 0;

  virtual uint32_t GetLKey() const = 0;

  virtual uint32_t GetRKey() const = 0;

};
}

#endif //RNETLIB_REGISTERED_MEMORY_H
