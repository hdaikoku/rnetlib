//
// Created by Harunobu Daikoku on 2017/02/15.
//

#ifndef RNETLIB_REMOTE_MEMORY_REGION_H
#define RNETLIB_REMOTE_MEMORY_REGION_H

namespace rnetlib {
class RemoteMemoryRegion {
 public:

  RemoteMemoryRegion(void *addr, uint32_t rkey, size_t length) : addr_(addr), rkey_(rkey), length_(length) {}

  virtual void *GetAddr() const {
    return addr_;
  }

  virtual uint32_t GetRKey() const {
    return rkey_;
  }

  virtual size_t GetLength() const {
    return length_;
  }

 private:
  void *addr_;
  uint32_t rkey_;
  size_t length_;

};
}

#endif //RNETLIB_REMOTE_MEMORY_REGION_H
