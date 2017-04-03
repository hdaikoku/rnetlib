//
// Created by Harunobu Daikoku on 2017/02/16.
//

#ifndef RNETLIB_EVENT_HANDLER_H
#define RNETLIB_EVENT_HANDLER_H

#define MAY_BE_REMOVED 1

namespace rnetlib {
class EventHandler {
 public:

  // keep this empty virtual destructor for derived classes
  virtual ~EventHandler() = default;

  virtual int OnEvent(int event_type, void *arg) = 0;

  virtual int OnError(int error_type) = 0;

  virtual void *GetHandlerID() const = 0;

  virtual short GetEventType() const = 0;

};
}

#endif //RNETLIB_EVENT_HANDLER_H
