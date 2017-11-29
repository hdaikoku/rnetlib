#ifndef RNETLIB_CLIENT_H_
#define RNETLIB_CLIENT_H_

#include "rnetlib/channel.h"

namespace rnetlib {

class Client {
 public:
  using ptr = std::unique_ptr<Client>;

  virtual ~Client() = default;

  virtual Channel::ptr Connect(const std::string &peer_addr, uint16_t peer_port, uint64_t peer_desc = 0) = 0;
};

} // namespace rnetlib

#endif // RNETLIB_CLIENT_H_
