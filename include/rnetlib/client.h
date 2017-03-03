//
// Created by Harunobu Daikoku on 2017/02/10.
//

#ifndef RNETLIB_CLIENT_H
#define RNETLIB_CLIENT_H

#include <functional>
#include <future>

#include "rnetlib/channel.h"
#include "rnetlib/event_loop.h"

namespace rnetlib {
class Client {
 public:

  virtual Channel::Ptr Connect() = 0;

  virtual std::future<Channel::Ptr> Connect(EventLoop &loop,
                                            std::function<void(const rnetlib::Channel &)> on_established = nullptr) = 0;

};
}

#endif //RNETLIB_CLIENT_H
