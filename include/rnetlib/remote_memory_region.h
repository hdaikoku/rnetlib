#ifndef RNETLIB_REMOTE_MEMORY_REGION_H_
#define RNETLIB_REMOTE_MEMORY_REGION_H_

namespace rnetlib {

class RemoteMemoryRegion {
 public:
  RemoteMemoryRegion(void *addr, uint32_t rkey, size_t length) : addr_(addr), rkey_(rkey), length_(length) {}

  void *GetAddr() const { return addr_; }

  uint32_t GetRKey() const { return rkey_; }

  size_t GetLength() const { return length_; }

 private:
  void *addr_;
  uint32_t rkey_;
  size_t length_;
};

} // namespace rnetlib

#endif // RNETLIB_REMOTE_MEMORY_REGION_H_
