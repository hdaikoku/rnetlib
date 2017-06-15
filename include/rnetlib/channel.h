//
// Created by Harunobu Daikoku on 2017/02/10.
//

#ifndef RNETLIB_CHANNEL_H
#define RNETLIB_CHANNEL_H

#include <memory>
#include <vector>

#include "rnetlib/local_memory_region.h"
#include "rnetlib/remote_memory_region.h"

namespace rnetlib {
class Channel {
 public:

  using Ptr = std::unique_ptr<Channel>;

  // keep this empty virtual destructor for derived classes
  virtual ~Channel() = default;

  virtual bool SetNonBlocking(bool non_blocking) = 0;

  virtual size_t Send(void *buf, size_t len) const = 0;

  virtual size_t Recv(void *buf, size_t len) const = 0;

  virtual size_t Send(LocalMemoryRegion &mem) const = 0;

  virtual size_t Recv(LocalMemoryRegion &mem) const = 0;

  virtual size_t SendSG(const std::vector<std::unique_ptr<LocalMemoryRegion>> &vec) const = 0;

  virtual size_t RecvSG(const std::vector<std::unique_ptr<LocalMemoryRegion>> &vec) const = 0;

  virtual size_t Write(LocalMemoryRegion &local_mem, RemoteMemoryRegion &remote_mem) const = 0;

  virtual size_t Read(LocalMemoryRegion &local_mem, RemoteMemoryRegion &remote_mem) const = 0;

  virtual std::unique_ptr<LocalMemoryRegion> RegisterMemory(void *addr, size_t len, int type) const = 0;

  virtual void SynRemoteMemoryRegion(LocalMemoryRegion &mem) const = 0;

  virtual std::unique_ptr<RemoteMemoryRegion> AckRemoteMemoryRegion() const = 0;

};
}

#endif //RNETLIB_CHANNEL_H