//
// Created by Harunobu Daikoku on 2017/02/15.
//

#ifndef RNETLIB_REMOTE_MEMORY_REGION_H
#define RNETLIB_REMOTE_MEMORY_REGION_H

namespace rnetlib {
class RemoteMemoryRegion {
 public:

  RemoteMemoryRegion(void *addr, uint32_t rkey) : addr_(addr), rkey_(rkey) {}

  virtual void *GetAddr() const {
    return addr_;
  }

  virtual uint32_t GetRKey() const {
    return rkey_;
  }

 private:
  void *addr_;
  uint32_t rkey_;

};
}

#endif //RNETLIB_REMOTE_MEMORY_REGION_H
