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
class RDMAChannel : public Channel, public RDMACommon {
 public:

  RDMAChannel(struct rdma_cm_id *id) : RDMACommon(id) {}

  size_t Send(void *buf, size_t len) const override {
    auto mem = RegisterMemory(buf, len, MR_LOCAL_WRITE);
    if (!mem) {
      // error
      return 0;
    }

    return Send(*mem);
  }

  size_t Recv(void *buf, size_t len) const override {
    auto mem = RegisterMemory(buf, len, MR_LOCAL_WRITE);
    if (!mem) {
      // error
      return 0;
    }

    return Recv(*mem);
  }

  size_t Send(LocalMemoryRegion &mem) const override {
    struct ibv_sge sge;
    struct ibv_send_wr wr, *bad_wr;
    auto len = mem.GetLength();

    std::memset(&sge, 0, sizeof(struct ibv_sge));
    sge.addr = reinterpret_cast<uintptr_t>(mem.GetAddr());
    sge.length = static_cast<uint32_t>(len);
    sge.lkey = mem.GetLKey();

    std::memset(&wr, 0, sizeof(struct ibv_send_wr));
    wr.sg_list = &sge;
    wr.num_sge = 1;
    wr.opcode = IBV_WR_SEND;
    if (len <= max_inline_data_) {
      wr.send_flags |= IBV_SEND_INLINE;
    }

    if (ibv_post_send(id_->qp, &wr, &bad_wr)) {
      // error
      return 0;
    }

    return PollCQ(id_->send_cq_channel, id_->send_cq) ? mem.GetLength() : 0;
  }

  size_t Recv(LocalMemoryRegion &mem) const override {
    struct ibv_sge sge;
    struct ibv_recv_wr wr, *bad_wr;

    std::memset(&sge, 0, sizeof(struct ibv_sge));
    sge.addr = reinterpret_cast<uintptr_t>(mem.GetAddr());
    sge.length = static_cast<uint32_t>(mem.GetLength());
    sge.lkey = mem.GetLKey();

    std::memset(&wr, 0, sizeof(struct ibv_recv_wr));
    wr.sg_list = &sge;
    wr.num_sge = 1;

    if (ibv_post_recv(id_->qp, &wr, &bad_wr)) {
      // error
      return 0;
    }

    return PollCQ(id_->recv_cq_channel, id_->recv_cq) ? mem.GetLength() : 0;
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

    if (ibv_post_send(id_->qp, &wr, &bad_wr)) {
      // error
      return 0;
    }

    return PollCQ(id_->send_cq_channel, id_->send_cq) ? local_mem.GetLength() : 0;
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

    if (ibv_post_send(id_->qp, &wr, &bad_wr)) {
      // error
      return 0;
    }

    return PollCQ(id_->send_cq_channel, id_->send_cq) ? local_mem.GetLength() : 0;
  }

  size_t PollWrite(LocalMemoryRegion &local_mem) const override {
    return local_mem.GetLength();
  }

  size_t PollRead(LocalMemoryRegion &local_mem) const override {
    return local_mem.GetLength();
  }

  std::unique_ptr<LocalMemoryRegion> RegisterMemory(void *addr, size_t len, int type) const override {
    return RDMALocalMemoryRegion::Register(id_->pd, addr, len, type);
  }

  void SynRemoteMemoryRegion(LocalMemoryRegion &mem) const override {
    uint64_t addr = reinterpret_cast<uintptr_t>(mem.GetAddr());
    uint32_t rkey = mem.GetRKey();
    auto size = sizeof(addr) + sizeof(rkey);

    std::unique_ptr<char[]> serialized(new char[size]);
    std::memcpy(serialized.get(), &addr, sizeof(addr));
    std::memcpy(serialized.get() + sizeof(addr), &rkey, sizeof(rkey));

    Send(serialized.get(), size);
  }

  std::unique_ptr<RemoteMemoryRegion> AckRemoteMemoryRegion() const override {
    uint64_t addr;
    uint32_t rkey;
    auto size = sizeof(addr) + sizeof(rkey);
    std::unique_ptr<char[]> serialized(new char[size]);

    // receive memory region info from the peer node
    if (Recv(serialized.get(), size) != size) {
      return nullptr;
    }
    // deserialize it
    std::memcpy(&addr, serialized.get(), sizeof(addr));
    std::memcpy(&rkey, serialized.get() + sizeof(addr), sizeof(rkey));

    return std::unique_ptr<RemoteMemoryRegion>(new RemoteMemoryRegion(reinterpret_cast<void *>(addr), rkey));
  }

 private:

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
