//
// Created by Harunobu Daikoku on 2017/02/16.
//

#ifndef RNETLIB_EVENT_LOOP_H
#define RNETLIB_EVENT_LOOP_H

#include "rnetlib/event_handler.h"

namespace rnetlib {
class EventLoop {
 public:
  static const int kErrTimedOut = 1;
  static const int kErrFailed = 2;

  virtual void AddHandler(std::unique_ptr<EventHandler> handler) = 0;

  virtual int Run(int timeout) = 0;

};
}

#endif //RNETLIB_EVENT_LOOP_H