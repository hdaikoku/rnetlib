#ifndef RNETLIB_CHANNEL_H_
#define RNETLIB_CHANNEL_H_

#include "rnetlib/event_loop.h"
#include "rnetlib/local_memory_region.h"
#include "rnetlib/remote_memory_region.h"

namespace rnetlib {

class Channel {
 public:
  using ptr = std::unique_ptr<Channel>;

  virtual ~Channel() = default;

  virtual uint64_t GetDesc() const = 0;

  virtual bool SetNonBlocking(bool non_blocking) = 0;

  virtual size_t Send(void *buf, size_t len) = 0;

  virtual size_t Recv(void *buf, size_t len) = 0;

  virtual size_t Send(const LocalMemoryRegion::ptr &lmr) = 0;

  virtual size_t Recv(const LocalMemoryRegion::ptr &lmr) = 0;

  virtual size_t ISend(void *buf, size_t len, EventLoop &evloop) = 0;

  virtual size_t IRecv(void *buf, size_t len, EventLoop &evloop) = 0;

  virtual size_t SendV(const LocalMemoryRegion::ptr *lmr, size_t lmrcnt) = 0;

  virtual size_t RecvV(const LocalMemoryRegion::ptr *lmr, size_t lmrcnt) = 0;

  virtual size_t ISendV(const LocalMemoryRegion::ptr *lmr, size_t lmrcnt, EventLoop &evloop) = 0;

  virtual size_t IRecvV(const LocalMemoryRegion::ptr *lmr, size_t lmrcnt, EventLoop &evloop) = 0;

  virtual size_t Write(void *buf, size_t len, const RemoteMemoryRegion &rmr) = 0;

  virtual size_t Read(void *buf, size_t len, const RemoteMemoryRegion &rmr) = 0;

  virtual size_t Write(const LocalMemoryRegion::ptr &lmr, const RemoteMemoryRegion &rmr) = 0;

  virtual size_t Read(const LocalMemoryRegion::ptr &lmr, const RemoteMemoryRegion &rmr) = 0;

  virtual size_t WriteV(const LocalMemoryRegion::ptr *lmr, const RemoteMemoryRegion *rmr, size_t cnt) = 0;

  virtual size_t ReadV(const LocalMemoryRegion::ptr *lmr, const RemoteMemoryRegion *rmr, size_t cnt) = 0;

  virtual LocalMemoryRegion::ptr RegisterMemoryRegion(void *addr, size_t len, int type) const = 0;

  virtual void SynRemoteMemoryRegionV(const LocalMemoryRegion::ptr *lmr, size_t lmrcnt) = 0;

  virtual void AckRemoteMemoryRegionV(RemoteMemoryRegion *rmr, size_t rmrcnt) = 0;
};

} // namespace rnetlib

#endif // RNETLIB_CHANNEL_H_