#ifndef RNETLIB_CLIENT_H_
#define RNETLIB_CLIENT_H_

#include <functional>
#include <future>

#include "rnetlib/channel.h"
#include "rnetlib/event_loop.h"

namespace rnetlib {

class Client {
 public:
  using ptr = std::unique_ptr<Client>;

  virtual ~Client() = default;

  virtual Channel::ptr Connect() = 0;
};

} // namespace rnetlib

#endif // RNETLIB_CLIENT_H_
