//
// Created by Harunobu Daikoku on 2017/02/10.
//

#ifndef RNETLIB_SERVER_H
#define RNETLIB_SERVER_H

#include <memory>

#include "rnetlib/channel.h"

namespace rnetlib {
class Server {
 public:

  virtual bool Listen() = 0;

  virtual Channel::Ptr Accept() = 0;

};
}

#endif //RNETLIB_SERVER_H
