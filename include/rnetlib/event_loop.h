#ifndef RNETLIB_EVENT_LOOP_H_
#define RNETLIB_EVENT_LOOP_H_

#include <memory>

#include "rnetlib/event_handler.h"

namespace rnetlib {

class EventLoop {
 public:
  using ptr = std::unique_ptr<EventLoop>;

  static const int kErrTimedOut = 1;
  static const int kErrFailed = 2;

  virtual ~EventLoop() = default;

  virtual void AddHandler(EventHandler &handler) = 0;

  virtual int WaitAll(int timeout_millis) = 0;
};

} // namespace rnetlib

#endif // RNETLIB_EVENT_LOOP_H_
