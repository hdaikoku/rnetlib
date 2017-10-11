#ifndef RNETLIB_CHANNEL_H_
#define RNETLIB_CHANNEL_H_

#include <memory>
#include <vector>

#include "rnetlib/event_loop.h"
#include "rnetlib/local_memory_region.h"
#include "rnetlib/remote_memory_region.h"

namespace rnetlib {

class Channel {
 public:
  using Ptr = std::unique_ptr<Channel>;

  virtual ~Channel() = default;

  virtual bool SetNonBlocking(bool non_blocking) = 0;

  virtual size_t Send(void *buf, size_t len) const = 0;

  virtual size_t Recv(void *buf, size_t len) const = 0;

  virtual size_t Send(LocalMemoryRegion &mem) const = 0;

  virtual size_t Recv(LocalMemoryRegion &mem) const = 0;

  virtual size_t ISend(void *buf, size_t len, EventLoop &evloop) = 0;

  virtual size_t IRecv(void *buf, size_t len, EventLoop &evloop) = 0;

  virtual size_t SendVec(const std::vector<std::unique_ptr<LocalMemoryRegion>> &vec) const = 0;

  virtual size_t RecvVec(const std::vector<std::unique_ptr<LocalMemoryRegion>> &vec) const = 0;

  virtual size_t ISendVec(const std::vector<std::unique_ptr<LocalMemoryRegion>> &vec, EventLoop &evloop) = 0;

  virtual size_t IRecvVec(const std::vector<std::unique_ptr<LocalMemoryRegion>> &vec, EventLoop &evloop) = 0;

  virtual size_t Write(LocalMemoryRegion &local_mem, RemoteMemoryRegion &remote_mem) const = 0;

  virtual size_t Read(LocalMemoryRegion &local_mem, RemoteMemoryRegion &remote_mem) const = 0;

  virtual std::unique_ptr<LocalMemoryRegion> RegisterMemory(void *addr, size_t len, int type) const = 0;

  virtual void SynRemoteMemoryRegion(const LocalMemoryRegion &mem) const = 0;

  virtual std::unique_ptr<RemoteMemoryRegion> AckRemoteMemoryRegion() const = 0;
};

} // namespace rnetlib

#endif // RNETLIB_CHANNEL_H_