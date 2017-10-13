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

  virtual std::future<Channel::ptr> Connect(EventLoop &loop,
                                            std::function<void(rnetlib::Channel &)> on_established = nullptr) = 0;
};

} // namespace rnetlib

#endif // RNETLIB_CLIENT_H_
