#ifndef RNETLIB_CLIENT_H_
#define RNETLIB_CLIENT_H_

#include <functional>
#include <future>

#include "rnetlib/channel.h"
#include "rnetlib/event_loop.h"

namespace rnetlib {

class Client {
 public:
  virtual ~Client() = default;

  virtual Channel::Ptr Connect() = 0;

  virtual std::future<Channel::Ptr> Connect(EventLoop &loop,
                                            std::function<void(rnetlib::Channel &)> on_established = nullptr) = 0;
};

} // namespace rnetlib

#endif // RNETLIB_CLIENT_H_
