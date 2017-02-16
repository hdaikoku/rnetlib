//
// Created by Harunobu Daikoku on 2017/02/10.
//

#ifndef RNETLIB_CLIENT_H
#define RNETLIB_CLIENT_H

#include <memory>

#include "rnetlib/channel.h"

namespace rnetlib {
class Client {
 public:

  virtual Channel::Ptr Connect() = 0;

};
}

#endif //RNETLIB_CLIENT_H
