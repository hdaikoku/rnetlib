#ifndef RNETLIB_EVENT_HANDLER_H_
#define RNETLIB_EVENT_HANDLER_H_

#define MAY_BE_REMOVED 1

namespace rnetlib {

class EventHandler {
 public:
  virtual ~EventHandler() = default;

  virtual int OnEvent(int event_type, void *arg) = 0;

  virtual int OnError(int error_type) = 0;

  virtual void *GetHandlerID() const = 0;

  virtual short GetEventType() const = 0;
};

} // namespace rnetlib

#endif // RNETLIB_EVENT_HANDLER_H_
