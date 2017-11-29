#ifndef RNETLIB_SERVER_H_
#define RNETLIB_SERVER_H_

#include "rnetlib/channel.h"

namespace rnetlib {

class Server {
 public:
  using ptr = std::unique_ptr<Server>;

  virtual ~Server() = default;
  
  virtual bool Listen() = 0;

  virtual Channel::ptr Accept() = 0;

  virtual std::string GetRawAddr() const = 0;

  virtual uint16_t GetListenPort() const = 0;
};

} // namespace rnetlib

#endif // RNETLIB_SERVER_H_
