//
// Created by Harunobu Daikoku on 2017/02/13.
//

#ifndef RNETLIB_REGISTERED_MEMORY_H
#define RNETLIB_REGISTERED_MEMORY_H

enum RMType {
  RM_LOCAL_WRITE = (1 << 0),
  RM_REMOTE_WRITE = (1 << 1),
  RM_REMOTE_READ = (1 << 2)
};

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
