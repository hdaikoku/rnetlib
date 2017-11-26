#ifndef RNETLIB_SERVER_H_
#define RNETLIB_SERVER_H_

#include <functional>
#include <future>

#include "rnetlib/channel.h"
#include "rnetlib/event_loop.h"

namespace rnetlib {

class Server {
 public:
  using ptr = std::unique_ptr<Server>;

  virtual ~Server() = default;
  
  virtual bool Listen() = 0;

  virtual Channel::ptr Accept() = 0;

  virtual std::future<Channel::ptr> Accept(EventLoop &loop,
                                           std::function<void(rnetlib::Channel &)> on_established = nullptr) = 0;

  virtual std::string GetRawAddr() const = 0;

  virtual uint16_t GetListenPort() const = 0;
};

} // namespace rnetlib

#endif // RNETLIB_SERVER_H_
