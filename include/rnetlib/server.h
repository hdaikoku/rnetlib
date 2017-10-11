#ifndef RNETLIB_SERVER_H_
#define RNETLIB_SERVER_H_

#include <functional>
#include <future>

#include "rnetlib/channel.h"
#include "rnetlib/event_loop.h"

namespace rnetlib {

class Server {
 public:
  virtual ~Server() = default;
  
  virtual bool Listen() = 0;

  virtual Channel::Ptr Accept() = 0;

  virtual std::future<Channel::Ptr> Accept(EventLoop &loop,
                                           std::function<void(rnetlib::Channel &)> on_established = nullptr) = 0;
};

} // namespace rnetlib

#endif // RNETLIB_SERVER_H_
