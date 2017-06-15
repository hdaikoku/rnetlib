//
// Created by Harunobu Daikoku on 2017/02/10.
//

#ifndef RNETLIB_SOCKET_SOCKET_COMMON_H
#define RNETLIB_SOCKET_SOCKET_COMMON_H

#include <cstring>
#include <fcntl.h>
#include <memory>
#include <netdb.h>
#include <string>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/uio.h>

#ifdef USE_RDMA
// rsocket-specific functions
#include <rdma/rsocket.h>

#define S_SOCKET(f, t, p)            rsocket(f, t, p)
#define S_CLOSE(f)                   rclose(f)
#define S_SHUTDOWN(f, h)             rshutdown(f, h)
#define S_WRITE(f, b, l)             rwrite(f, b, l)
#define S_READ(f, b, l)              rread(f, b, l)
#define S_WRITEV(f, v, c)            rwritev(f, v, c)
#define S_READV(f, v, c)             rreadv(f, v, c)
#define S_FCNTL(s,c,p)               rfcntl(s, c, p)
#define S_ADDRINFO                   rdma_addrinfo
#define S_GETADDRINFO(a, p, h, r)    rdma_getaddrinfo(a, p, h, r)
#define S_FREEADDRINFO(r)            rdma_freeaddrinfo(r)
#define S_SETSOCKOPT(s, l, n, v, ol) rsetsockopt(s, l, n, v, ol)
#define S_GETSOCKOPT(s, l, n, v, ol) rgetsockopt(s, l, n, v, ol)
#define S_POLL(f, n, t)              rpoll(f, n, t)
#define S_GETPEERNAME(s, a, l)       rgetpeername(s, a, l)
#else
// BSD-socket specific functions
#define S_SOCKET(f, t, p)            ::socket(f, t, p)
#define S_CLOSE(f)                   close(f)
#define S_SHUTDOWN(f, h)             shutdown(f, h)
#define S_WRITE(f, b, l)             write(f, b, l)
#define S_READ(f, b, l)              read(f, b, l)
#define S_WRITEV(f, v, c)            writev(f, v, c)
#define S_READV(f, v, c)             readv(f, v, c)
#define S_FCNTL(s, c, p)             fcntl(s, c, p)
#define S_ADDRINFO                   addrinfo
#define S_GETADDRINFO(a, p, h, r)    getaddrinfo(a, p, h, r)
#define S_FREEADDRINFO(r)            freeaddrinfo(r)
#define S_SETSOCKOPT(s, l, n, v, ol) setsockopt(s, l, n, v, ol)
#define S_GETSOCKOPT(s, l, n, v, ol) getsockopt(s, l, n, v, ol)
#define S_POLL(f, n, t)              poll(f, n, t)
#define S_GETPEERNAME(s, a, l)       getpeername(s, a, l)
#endif //USE_RDMA

namespace rnetlib {
namespace socket {
class SocketCommon {
 public:
  // deleter class for (rdma_)addrinfo
  class AddrInfoDeleter {
   public:
    void operator()(struct S_ADDRINFO *p) const {
      if (p) {
        S_FREEADDRINFO(p);
      }
    }
  };

  SocketCommon() = default;

  SocketCommon(int sock_fd) : sock_fd_(sock_fd) {}

 protected:
  int sock_fd_;

  void Close() {
    S_SHUTDOWN(sock_fd_, SHUT_RDWR);
    S_CLOSE(sock_fd_);
  }

  std::unique_ptr<struct S_ADDRINFO, AddrInfoDeleter> Open(const char *addr, uint16_t port, int flags) {
    struct S_ADDRINFO hints;
    std::unique_ptr<struct S_ADDRINFO, AddrInfoDeleter> results;

    std::memset(&hints, 0, sizeof(struct S_ADDRINFO));
#ifdef USE_RDMA
    hints.ai_flags = flags | RAI_FAMILY;
    hints.ai_port_space = RDMA_PS_TCP;
#else
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_INET;
    hints.ai_flags = flags;
#endif //USE_RDMA

    struct S_ADDRINFO *tmp;
    int error = S_GETADDRINFO(const_cast<char *>(addr), const_cast<char *>(std::to_string(port).c_str()), &hints, &tmp);
    if (error) {
      if (error == EAI_SYSTEM) {
        // TODO: log error
        // LogError(errno);
      } else {
        // TODO: log error
        // LogError(gai_strerror(error));
      }
      return nullptr;
    }
    results.reset(tmp);

    if ((sock_fd_ = S_SOCKET(results->ai_family, SOCK_STREAM, 0)) == -1) {
      // TODO: log error
      return nullptr;
    }

    // TODO: TCP_NODELAY should be user-configurable
    int no_delay = 1;
    if (!SetSockOpt(IPPROTO_TCP, TCP_NODELAY, no_delay)) {
      Close();
      return nullptr;
    }

    // TODO: buf_size should be user-configurable (buf_size = RTT * BW)
    int buf_size = (1 << 21);
    if (!SetSockOpt(SOL_SOCKET, SO_SNDBUF, buf_size)) {
      Close();
      return nullptr;
    }
    if (!SetSockOpt(SOL_SOCKET, SO_RCVBUF, buf_size)) {
      Close();
      return nullptr;
    }

#ifdef USE_RDMA
    // TODO: RDMA_INLINE should be user-configurable
    int val = 0;
    if (!SetSockOpt(SOL_RDMA, RDMA_INLINE, val)) {
      Close();
      return nullptr;
    }
#endif //USE_RDMA

    return std::move(results);
  }

  bool SetSockOpt(int level, int option_name, int val) {
    return (S_SETSOCKOPT(sock_fd_, level, option_name, &val, sizeof(val)) == 0);
  }

  bool GetSockOpt(int level, int option_name, int &val) {
    socklen_t len;
    return (S_GETSOCKOPT(sock_fd_, level, option_name, &val, &len) == 0);
  }

  bool SetNonBlocking(bool non_blocking) {
    if (non_blocking) {
      S_FCNTL(sock_fd_, F_SETFL, S_FCNTL(sock_fd_, F_GETFL, 0) | O_NONBLOCK);
    } else {
      S_FCNTL(sock_fd_, F_SETFL, S_FCNTL(sock_fd_, F_GETFL, 0) & ~O_NONBLOCK);
    }

    return true;
  }

};
}
}

#endif //RNETLIB_SOCKET_SOCKET_COMMON_H
