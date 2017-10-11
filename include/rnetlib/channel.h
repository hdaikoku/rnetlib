#ifndef RNETLIB_CHANNEL_H_
#define RNETLIB_CHANNEL_H_

#include <vector>

#include "rnetlib/event_loop.h"
#include "rnetlib/local_memory_region.h"
#include "rnetlib/remote_memory_region.h"

namespace rnetlib {

class Channel {
 public:
  using ptr = std::unique_ptr<Channel>;

  virtual ~Channel() = default;

  virtual bool SetNonBlocking(bool non_blocking) = 0;

  virtual size_t Send(void *buf, size_t len) const = 0;

  virtual size_t Recv(void *buf, size_t len) const = 0;

  virtual size_t Send(const LocalMemoryRegion &mem) const = 0;

  virtual size_t Recv(const LocalMemoryRegion &mem) const = 0;

  virtual size_t ISend(void *buf, size_t len, EventLoop &evloop) = 0;

  virtual size_t IRecv(void *buf, size_t len, EventLoop &evloop) = 0;

  virtual size_t SendV(const std::vector<LocalMemoryRegion::ptr> &vec) const = 0;

  virtual size_t RecvV(const std::vector<LocalMemoryRegion::ptr> &vec) const = 0;

  virtual size_t ISendV(const std::vector<LocalMemoryRegion::ptr> &vec, EventLoop &evloop) = 0;

  virtual size_t IRecvV(const std::vector<LocalMemoryRegion::ptr> &vec, EventLoop &evloop) = 0;

  virtual size_t Write(const LocalMemoryRegion &local_mem, const RemoteMemoryRegion &remote_mem) const = 0;

  virtual size_t Read(const LocalMemoryRegion &local_mem, const RemoteMemoryRegion &remote_mem) const = 0;

  virtual LocalMemoryRegion::ptr RegisterMemory(void *addr, size_t len, int type) const = 0;

  virtual void SynRemoteMemoryRegion(const LocalMemoryRegion &lmr) const = 0;

  virtual void AckRemoteMemoryRegion(RemoteMemoryRegion *rmr) const = 0;

  virtual void SynRemoteMemoryRegionV(const std::vector<LocalMemoryRegion::ptr> &vec) const = 0;

  virtual void AckRemoteMemoryRegionV(std::vector<RemoteMemoryRegion> *vec) const = 0;
};

} // namespace rnetlib

#endif // RNETLIB_CHANNEL_H_