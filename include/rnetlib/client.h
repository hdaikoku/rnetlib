//
// Created by Harunobu Daikoku on 2017/02/10.
//

#ifndef RNETLIB_CLIENT_CHANNEL_H
#define RNETLIB_CLIENT_CHANNEL_H

#include <memory>

#include "rnetlib/channel.h"

namespace rnetlib {
class Client {
 public:

  virtual std::unique_ptr<Channel> Connect(const std::string &peer_addr, uint16_t peer_port) = 0;

};
}

#endif //RNETLIB_CLIENT_CHANNEL_H
