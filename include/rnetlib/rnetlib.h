#ifndef RNETLIB_RNETLIB_H_
#define RNETLIB_RNETLIB_H_

#include "rnetlib/client.h"
#include "rnetlib/event_loop.h"
#include "rnetlib/server.h"
#include "rnetlib/socket/socket_client.h"
#include "rnetlib/socket/socket_event_loop.h"
#include "rnetlib/socket/socket_server.h"

#ifdef RNETLIB_ENABLE_OFI
#include "rnetlib/ofi/ofi_client.h"
#include "rnetlib/ofi/ofi_server.h"
#endif // RNETLIB_ENABLE_OFI

#ifdef RNETLIB_ENABLE_VERBS
#include "rnetlib/verbs/verbs_client.h"
#include "rnetlib/verbs/verbs_event_loop.h"
#include "rnetlib/verbs/verbs_server.h"
#endif // RNETLIB_ENABLE_VERBS

namespace rnetlib {

enum Prov {
  PROV_OFI,
  PROV_VERBS,
  PROV_SOCKET
};

static std::unique_ptr<Client> NewClient(Prov prov, uint64_t self_desc = 0) {
#ifdef RNETLIB_ENABLE_OFI
  if (prov == PROV_OFI) {
    return Client::ptr(new ofi::OFIClient(self_desc));
  }
#endif // RNETLIB_ENABLE_OFI

#ifdef RNETLIB_ENABLE_VERBS
  if (prov == PROV_VERBS) {
    return Client::ptr(new verbs::VerbsClient(self_desc));
  }
#endif // RNETLIB_ENABLE_VERBS

  return Client::ptr(new socket::SocketClient(self_desc));
}

static std::unique_ptr<Server> NewServer(const std::string &addr, uint16_t port, Prov prov) {
#ifdef RNETLIB_ENABLE_OFI
  if (prov == PROV_OFI) {
    return Server::ptr(new ofi::OFIServer(addr, port));
  }
#endif // RNETLIB_ENABLE_OFI

#ifdef RNETLIB_ENABLE_VERBS
  if (prov == PROV_VERBS) {
    return Server::ptr(new verbs::VerbsServer(addr, port));
  }
#endif // RNETLIB_ENABLE_VERBS

  return Server::ptr(new socket::SocketServer(addr, port));
}

static std::unique_ptr<EventLoop> NewEventLoop(Prov prov) {
#ifdef RNETLIB_ENABLE_VERBS
  if (prov == PROV_VERBS) {
    return std::unique_ptr<EventLoop>(new verbs::VerbsEventLoop);
  }
#endif // RNETLIB_ENABLE_VERBS

  return std::unique_ptr<EventLoop>(new socket::SocketEventLoop);
}

} // namespace rnetlib

#endif // RNETLIB_RNETLIB_H_
