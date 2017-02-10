//
// Created by Harunobu Daikoku on 2017/02/10.
//

#ifndef RNETLIB_CHANNEL_H
#define RNETLIB_CHANNEL_H

namespace rnetlib {
class Channel {
 public:

  virtual size_t Send(const void *buf, size_t len) const = 0;

  virtual size_t Recv(void *buf, size_t len) const = 0;

};
}

#endif //RNETLIB_CHANNEL_H