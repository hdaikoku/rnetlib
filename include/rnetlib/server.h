//
// Created by Harunobu Daikoku on 2017/02/10.
//

#ifndef RNETLIB_SERVER_CHANNEL_H
#define RNETLIB_SERVER_CHANNEL_H

#include <memory>

#include "rnetlib/channel.h"

namespace rnetlib {
class Server {
 public:

  virtual bool Listen(uint16_t server_port) = 0;

  virtual std::unique_ptr<Channel> Accept() = 0;

};
}

#endif //PROJECT_SERVER_CHANNEL_H
