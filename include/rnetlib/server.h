//
// Created by Harunobu Daikoku on 2017/02/10.
//

#ifndef RNETLIB_SERVER_H
#define RNETLIB_SERVER_H

#include <functional>
#include <future>

#include "rnetlib/channel.h"
#include "rnetlib/event_loop.h"

namespace rnetlib {
class Server {
 public:

  virtual bool Listen() = 0;

  virtual Channel::Ptr Accept() = 0;

  virtual std::future<Channel::Ptr> Accept(EventLoop &loop,
                                           std::function<void(const rnetlib::Channel &)> on_established = nullptr) = 0;

};
}

#endif //RNETLIB_SERVER_H
