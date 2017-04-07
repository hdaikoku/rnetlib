//
// Created by Harunobu Daikoku on 2017/02/10.
//

#ifndef RNETLIB_RDMA_RDMA_CHANNEL_H
#define RNETLIB_RDMA_RDMA_CHANNEL_H

#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <infiniband/verbs.h>

#include "rnetlib/channel.h"
#include "rnetlib/rdma/rdma_common.h"
#include "rnetlib/rdma/rdma_local_memory_region.h"

namespace rnetlib {
namespace rdma {
class RDMAChannel : public Channel {
 public:

  RDMAChannel(RDMACommon::RDMACommID id)
      : id_(std::move(id)), send_buf_(1 << 10, id_->pd), recv_buf_(1 << 10, id_->pd) {
    struct ibv_qp_attr attr;
    struct ibv_qp_init_attr init_attr;

    ibv_query_qp(id_->qp, &attr, 0, &init_attr);
    max_inline_data_ = attr.cap.max_inline_data;
    max_send_wr_ = attr.cap.max_send_wr;

    struct ibv_recv_wr *bad_wr;
    ibv_post_recv(id_->qp, const_cast<struct ibv_recv_wr *>(recv_buf_.GetWorkRequestPtr()), &bad_wr);
  }

  virtual ~RDMAChannel() {
    if (id_) {
      rdma_disconnect(id_.get());
    }
  }

  bool SetNonBlocking(bool non_blocking) override {
    return true;
  }

  size_t Send(void *buf, size_t len) const override {
    size_t sent, offset = 0;

    while (offset < len) {
      if ((len - offset) <= max_inline_data_) {
        // use inline data transfer for better latency.
        struct ibv_sge sge;
        struct ibv_send_wr wr, *bad_wr;

        std::memset(&sge, 0, sizeof(struct ibv_sge));
        sge.addr = reinterpret_cast<uintptr_t>(buf + offset);
        sge.length = static_cast<uint32_t>(len - offset);

        std::memset(&wr, 0, sizeof(struct ibv_send_wr));
        wr.sg_list = &sge;
        wr.num_sge = 1;
        wr.opcode = IBV_WR_SEND;
        wr.send_flags |= IBV_SEND_INLINE;

        sent = (len - offset);
        if (!PostSend(&wr)) {
          // failed to send
          break;
        }
      } else {
        // copy to pinned-down inner buffer and send.
        sent = send_buf_.Write(buf + offset, (len - offset));
        if (!PostSend(const_cast<struct ibv_send_wr *>(send_buf_.GetWorkRequestPtr()))) {
          // failed to send
          break;
        }
      }

      offset += sent;
    }

    return offset;
  }

  size_t Recv(void *buf, size_t len) const override {
    size_t offset = 0;
    struct ibv_recv_wr *bad_wr;

    while (offset < len) {
      if (ibv_post_recv(id_->qp, const_cast<struct ibv_recv_wr *>(recv_buf_.GetWorkRequestPtr()), &bad_wr)) {
        // error
        break;
      }

      if (!PollCQ(id_->recv_cq)) {
        // error
        break;
      }

      offset += recv_buf_.Read(buf + offset, len - offset);
    }

    return offset;
  }

  size_t Send(LocalMemoryRegion &mem) const override {
    return Send(mem.GetAddr(), mem.GetLength());
  }

  size_t Recv(LocalMemoryRegion &mem) const override {
    return Recv(mem.GetAddr(), mem.GetLength());
  }

  size_t Write(LocalMemoryRegion &local_mem, RemoteMemoryRegion &remote_mem) const override {
    struct ibv_sge sge;
    struct ibv_send_wr wr, *bad_wr;
    auto len = local_mem.GetLength();

    std::memset(&sge, 0, sizeof(struct ibv_sge));
    sge.addr = reinterpret_cast<uintptr_t>(local_mem.GetAddr());
    sge.length = static_cast<uint32_t>(len);
    sge.lkey = local_mem.GetLKey();

    std::memset(&wr, 0, sizeof(struct ibv_send_wr));
    wr.sg_list = &sge;
    wr.num_sge = 1;
    wr.opcode = IBV_WR_RDMA_WRITE;
    wr.wr.rdma.rkey = remote_mem.GetRKey();
    wr.wr.rdma.remote_addr = reinterpret_cast<uintptr_t>(remote_mem.GetAddr());
    if (len <= max_inline_data_) {
      wr.send_flags |= IBV_SEND_INLINE;
    }

    return PostSend(&wr) ? local_mem.GetLength() : 0;
  }

  size_t Read(LocalMemoryRegion &local_mem, RemoteMemoryRegion &remote_mem) const override {
    struct ibv_sge sge;
    struct ibv_send_wr wr, *bad_wr;
    auto len = local_mem.GetLength();

    std::memset(&sge, 0, sizeof(struct ibv_sge));
    sge.addr = reinterpret_cast<uintptr_t>(local_mem.GetAddr());
    sge.length = static_cast<uint32_t>(len);
    sge.lkey = local_mem.GetLKey();

    std::memset(&wr, 0, sizeof(struct ibv_send_wr));
    wr.sg_list = &sge;
    wr.num_sge = 1;
    wr.opcode = IBV_WR_RDMA_READ;
    wr.wr.rdma.rkey = remote_mem.GetRKey();
    wr.wr.rdma.remote_addr = reinterpret_cast<uintptr_t>(remote_mem.GetAddr());

    return PostSend(&wr) ? local_mem.GetLength() : 0;
  }

  std::unique_ptr<LocalMemoryRegion> RegisterMemory(void *addr, size_t len, int type) const override {
    return RDMALocalMemoryRegion::Register(id_->pd, addr, len, type);
  }

  void SynRemoteMemoryRegion(LocalMemoryRegion &mem) const override {
    uint64_t addr = reinterpret_cast<uintptr_t>(mem.GetAddr());
    uint32_t rkey = mem.GetRKey();
    size_t length = mem.GetLength();
    auto size = sizeof(addr) + sizeof(rkey) + sizeof(length);

    std::unique_ptr<char[]> serialized(new char[size]);
    std::memcpy(serialized.get(), &addr, sizeof(addr));
    std::memcpy(serialized.get() + sizeof(addr), &rkey, sizeof(rkey));
    std::memcpy(serialized.get() + sizeof(addr) + sizeof(rkey), &length, sizeof(length));

    Send(serialized.get(), size);
  }

  std::unique_ptr<RemoteMemoryRegion> AckRemoteMemoryRegion() const override {
    uint64_t addr;
    uint32_t rkey;
    size_t length;
    auto size = sizeof(addr) + sizeof(rkey) + sizeof(length);
    std::unique_ptr<char[]> serialized(new char[size]);

    // receive memory region info from the peer node
    if (Recv(serialized.get(), size) != size) {
      return nullptr;
    }
    // deserialize it
    std::memcpy(&addr, serialized.get(), sizeof(addr));
    std::memcpy(&rkey, serialized.get() + sizeof(addr), sizeof(rkey));
    std::memcpy(&length, serialized.get() + sizeof(addr) + sizeof(rkey), sizeof(length));

    return std::unique_ptr<RemoteMemoryRegion>(new RemoteMemoryRegion(reinterpret_cast<void *>(addr), rkey, length));
  }

 private:

  class BaseBuffer {
   public:

    BaseBuffer(uint32_t len, struct ibv_pd *pd, MRType mrtype)
        : buflen_(len), buf_(new char[len]),
          mr_(RDMALocalMemoryRegion::Register(pd, buf_.get(), buflen_, mrtype)) {
      std::memset(&sge_, 0, sizeof(struct ibv_sge));
      sge_.addr = reinterpret_cast<uintptr_t>(mr_->GetAddr());
      sge_.length = static_cast<uint32_t>(mr_->GetLength());
      sge_.lkey = mr_->GetLKey();
    }

    virtual ~BaseBuffer() = default;

    size_t Write(const void *addr, size_t len) const {
      auto cpylen = (len > buflen_) ? buflen_ : len;
      std::memcpy(buf_.get(), addr, cpylen);

      return cpylen;
    }

    size_t Read(void *addr, size_t len) const {
      auto cpylen = (len > buflen_) ? buflen_ : len;
      std::memcpy(addr, buf_.get(), cpylen);

      return cpylen;
    }

   protected:
    struct ibv_sge sge_;

   private:
    const uint32_t buflen_;
    const std::unique_ptr<char[]> buf_;
    const std::unique_ptr<LocalMemoryRegion> mr_;

  };

  class SendBuffer : public BaseBuffer {
   public:

    SendBuffer(uint32_t len, struct ibv_pd *pd) : BaseBuffer(len, pd, MR_LOCAL_READ) {
      std::memset(&wr_, 0, sizeof(struct ibv_send_wr));
      wr_.sg_list = &sge_;
      wr_.num_sge = 1;
      wr_.opcode = IBV_WR_SEND;
    }

    const struct ibv_send_wr *GetWorkRequestPtr() const {
      return &wr_;
    }

   private:
    struct ibv_send_wr wr_;

  };

  class RecvBuffer : public BaseBuffer {
   public:

    RecvBuffer(uint32_t len, struct ibv_pd *pd) : BaseBuffer(len, pd, MR_LOCAL_WRITE) {
      std::memset(&wr_, 0, sizeof(struct ibv_recv_wr));
      wr_.sg_list = &sge_;
      wr_.num_sge = 1;
    }

    const struct ibv_recv_wr *GetWorkRequestPtr() const {
      return &wr_;
    }

   private:
    struct ibv_recv_wr wr_;

  };

 private:
  RDMACommon::RDMACommID id_;
  uint32_t max_inline_data_;
  uint32_t max_send_wr_;
  SendBuffer send_buf_;
  RecvBuffer recv_buf_;

  bool PostSend(struct ibv_send_wr *wr) const {
    struct ibv_send_wr *bad_wr;

    if (ibv_post_send(id_->qp, wr, &bad_wr)) {
      // error
      return false;
    }

    return PollCQ(id_->send_cq);
  }

  bool PollCQ(struct ibv_comp_channel *channel, struct ibv_cq *cq) const {
    // request event notifications
    ibv_req_notify_cq(cq, 0);
    // set the file descriptor associated with the channel to non-blocking mode
    fcntl(channel->fd, F_SETFL, fcntl(channel->fd, F_GETFL) | O_NONBLOCK);

    struct pollfd pfd;
    std::memset(&pfd, 0, sizeof(struct pollfd));
    pfd.fd = channel->fd;
    pfd.events = POLLIN;

    int rc = 0;
    do {
      rc = poll(&pfd, 1, 300);
    } while (rc == 0);

    if (rc < 0 || pfd.revents != POLLIN) {
      // error
      return false;
    }

    struct ibv_cq *ev_cq;
    void *ev_ctx;
    if (ibv_get_cq_event(channel, &ev_cq, &ev_ctx)) {
      // error
      return false;
    }
    if (ev_cq != cq) {
      // unknown CQ
      return false;
    }

    ibv_ack_cq_events(cq, 1);

    return PollCQ(cq);
  }

  bool PollCQ(struct ibv_cq *cq) const {
    struct ibv_wc wc;

    while (ibv_poll_cq(cq, 1, &wc) < 1) {
      // poll
    }

    return (wc.status == IBV_WC_SUCCESS);
  }

};
}
}

#endif //RNETLIB_RDMA_RDMA_CHANNEL_H
