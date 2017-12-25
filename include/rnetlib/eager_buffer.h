#ifndef RNETLIB_EAGER_BUFFER_H_
#define RNETLIB_EAGER_BUFFER_H_

#include "rnetlib/channel.h"

#define EAGER_THRESHOLD 524288

namespace rnetlib {

class EagerBuffer {
 public:
  explicit EagerBuffer(const Channel &ch)
      : buf_(new char[EAGER_THRESHOLD]),
        lmr_(ch.RegisterMemoryRegion(buf_.get(), EAGER_THRESHOLD, MR_LOCAL_WRITE | MR_LOCAL_READ)) {}

  size_t Read(void *addr, size_t len) const {
    auto cpylen = (len > EAGER_THRESHOLD) ? EAGER_THRESHOLD : len;
    std::memcpy(addr, buf_.get(), cpylen);
    return cpylen;
  }

  size_t Write(void *addr, size_t len) {
    auto cpylen = (len > EAGER_THRESHOLD) ? EAGER_THRESHOLD : len;
    std::memcpy(buf_.get(), addr, cpylen);
    return cpylen;
  }

  void *GetAddr() const { return buf_.get(); }

  void *GetLKey() const { return lmr_->GetLKey(); }

 private:
  const std::unique_ptr<char[]> buf_;
  LocalMemoryRegion::ptr lmr_;
};

} // namespace rnetlib

#endif // RNETLIB_EAGER_BUFFER_H_
