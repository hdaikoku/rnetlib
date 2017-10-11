#ifndef RNETLIB_VERBS_VERBS_CHANNEL_H_
#define RNETLIB_VERBS_VERBS_CHANNEL_H_

#include <fcntl.h>
#include <poll.h>
#include <infiniband/verbs.h>

#include <cstring>

#include "rnetlib/channel.h"
#include "rnetlib/verbs/verbs_common.h"
#include "rnetlib/verbs/verbs_local_memory_region.h"

#define IBV_RCVBUF 32

namespace rnetlib {
namespace verbs {

class VerbsChannel : public Channel {
 public:
  explicit VerbsChannel(VerbsCommon::RDMACommID id) : id_(std::move(id)), recv_buf_(IBV_RCVBUF, id_->pd) {
    struct ibv_qp_attr attr;
    struct ibv_qp_init_attr init_attr;

    ibv_query_qp(id_->qp, &attr, 0, &init_attr);
    max_inline_data_ = attr.cap.max_inline_data;
    max_send_wr_ = attr.cap.max_send_wr;
    max_send_sge_ = attr.cap.max_send_sge;
    max_recv_sge_ = attr.cap.max_recv_sge;

    struct ibv_recv_wr *bad_wr;
    ibv_post_recv(id_->qp, const_cast<struct ibv_recv_wr *>(recv_buf_.GetWorkRequestPtr()), &bad_wr);
  }

  virtual ~VerbsChannel() {
    if (id_) {
      rdma_disconnect(id_.get());
    }
  }

  bool SetNonBlocking(bool non_blocking) override {
    return true;
  }

  size_t Send(void *buf, size_t len) const override {
    auto mr = RegisterMemory(buf, len, MR_LOCAL_READ);
    return Send(*mr);
  }

  size_t Recv(void *buf, size_t len) const override {
    auto mr = RegisterMemory(buf, len, MR_LOCAL_WRITE);
    return Recv(*mr);
  }

  size_t Send(const LocalMemoryRegion &mem) const override {
    size_t sending_len, offset = 0;
    auto buf = mem.GetAddr();
    auto len = mem.GetLength();

    sending_len = (len < IBV_RCVBUF) ? len : IBV_RCVBUF;
    offset = PostSend(IBV_WR_SEND, buf, sending_len, mem.GetLKey(), nullptr, 0);
    if (offset != sending_len) {
      // error
      return 0;
    }

    if (len > offset) {
      sending_len = len - offset;
      offset += PostSend(IBV_WR_SEND, buf + offset, sending_len, mem.GetLKey(), nullptr, 0);
    }

    return offset;
  }

  size_t Recv(const LocalMemoryRegion &mem) const override {
    size_t offset = 0;
    struct ibv_sge sge;
    struct ibv_recv_wr wr, *bad_wr;
    auto buf = mem.GetAddr();
    auto len = mem.GetLength();

    if (len > IBV_RCVBUF) {
      // post a recv request for the body part of the message in advance.
      std::memset(&sge, 0, sizeof(sge));
      sge.addr = reinterpret_cast<uintptr_t>(buf + IBV_RCVBUF);
      sge.length = static_cast<uint32_t>(len - IBV_RCVBUF);
      sge.lkey = mem.GetLKey();

      std::memset(&wr, 0, sizeof(wr));
      wr.sg_list = &sge;
      wr.num_sge = 1;

      ibv_post_recv(id_->qp, &wr, &bad_wr);
    }

    // refill a recv request for a header.
    ibv_post_recv(id_->qp, const_cast<struct ibv_recv_wr *>(recv_buf_.GetWorkRequestPtr()), &bad_wr);

    // receive the header part.
    if (!PollCQ(id_->recv_cq)) {
      // error
      return offset;
    }

    if (len > IBV_RCVBUF) {
      // receive the body part.
      if (!PollCQ(id_->recv_cq)) {
        // error
        return offset;
      }
      offset += (len - IBV_RCVBUF);
    }

    // copy the received header part to the user buffer.
    offset += recv_buf_.Read(buf, len < IBV_RCVBUF ? len : IBV_RCVBUF);

    return offset;
  }

  size_t ISend(void *buf, size_t len, EventLoop &evloop) override {
    // TODO: implement this.
    return 0;
  }

  size_t IRecv(void *buf, size_t len, EventLoop &evloop) override {
    // TODO: implement this.
    return 0;
  }

  size_t SendV(const std::vector<std::unique_ptr<LocalMemoryRegion>> &vec) const override {
    // we cannot use the Scatter/Gather function of IBV here because we use pre-allocated buffers for Send/Recv.
    size_t offset = 0, len = 0;
    for (const auto &mr : vec) {
      len += mr->GetLength();
    }
    std::unique_ptr<char[]> buf(new char[len]);
    for (const auto &mr : vec) {
      std::memcpy(buf.get() + offset, mr->GetAddr(), mr->GetLength());
      offset += mr->GetLength();
    }

    return Send(buf.get(), len);
  }

  size_t RecvV(const std::vector<std::unique_ptr<LocalMemoryRegion>> &vec) const override {
    // we cannot use the Scatter/Gather function of IBV here because we use pre-allocated buffers for Send/Recv.
    size_t offset = 0, len = 0;
    for (const auto &mr : vec) {
      len += mr->GetLength();
    }
    std::unique_ptr<char[]> buf(new char[len]);
    if (Recv(buf.get(), len) != len) {
      // error
      return 0;
    }

    for (const auto &mr : vec) {
      std::memcpy(mr->GetAddr(), buf.get() + offset, mr->GetLength());
      offset += mr->GetLength();
    }

    return len;
  }

  size_t ISendV(const std::vector<std::unique_ptr<LocalMemoryRegion>> &vec, EventLoop &evloop) override {
    // TODO: implement this.
    return 0;
  }

  size_t IRecvV(const std::vector<std::unique_ptr<LocalMemoryRegion>> &vec, EventLoop &evloop) override {
    // TODO: implement this.
    return 0;
  }

  size_t Write(const LocalMemoryRegion &local_mem, const RemoteMemoryRegion &remote_mem) const override {
    return PostSend(IBV_WR_RDMA_WRITE, local_mem.GetAddr(), local_mem.GetLength(), local_mem.GetLKey(),
                    reinterpret_cast<void *>(remote_mem.addr), remote_mem.rkey);
  }

  size_t Read(const LocalMemoryRegion &local_mem, const RemoteMemoryRegion &remote_mem) const override {
    return PostSend(IBV_WR_RDMA_READ, local_mem.GetAddr(), local_mem.GetLength(), local_mem.GetLKey(),
                    reinterpret_cast<void *>(remote_mem.addr), remote_mem.rkey);
  }

  std::unique_ptr<LocalMemoryRegion> RegisterMemory(void *addr, size_t len, int type) const override {
    return VerbsLocalMemoryRegion::Register(id_->pd, addr, len, type);
  }

  void SynRemoteMemoryRegion(const LocalMemoryRegion &lmr) const override {
    RemoteMemoryRegion rmr(lmr);

    Send(&rmr, sizeof(rmr));
  }

  void AckRemoteMemoryRegion(RemoteMemoryRegion *rmr) const override { Recv(rmr, sizeof(RemoteMemoryRegion)); }

  void SynRemoteMemoryRegionV(const std::vector<std::unique_ptr<LocalMemoryRegion>> &vec) const override {
    std::vector<RemoteMemoryRegion> rmrs;
    rmrs.reserve(vec.size());

    for (const auto &v : vec) {
      rmrs.emplace_back(*v);
    }

    Send(rmrs.data(), sizeof(RemoteMemoryRegion) * rmrs.size());
  }

  void AckRemoteMemoryRegionV(std::vector<RemoteMemoryRegion> *vec) const override {
    Recv(vec->data(), sizeof(RemoteMemoryRegion) * vec->size());
  }

  const struct rdma_cm_id *GetIDPtr() const {
    return id_.get();
  }

 private:
  // Pre-allocated buffer for Recv operations.
  class RecvBuffer {
   public:

    RecvBuffer(uint32_t len, struct ibv_pd *pd)
        : buflen_(len), buf_(new char[len]),
          mr_(VerbsLocalMemoryRegion::Register(pd, buf_.get(), buflen_, MR_LOCAL_WRITE)) {
      std::memset(&sge_, 0, sizeof(struct ibv_sge));
      sge_.addr = reinterpret_cast<uintptr_t>(mr_->GetAddr());
      sge_.length = static_cast<uint32_t>(mr_->GetLength());
      sge_.lkey = mr_->GetLKey();
      std::memset(&wr_, 0, sizeof(struct ibv_recv_wr));
      wr_.sg_list = &sge_;
      wr_.num_sge = 1;
    }

    size_t Read(void *addr, size_t len) const {
      auto cpylen = (len > buflen_) ? buflen_ : len;
      std::memcpy(addr, buf_.get(), cpylen);

      return cpylen;
    }

    const struct ibv_recv_wr *GetWorkRequestPtr() const {
      return &wr_;
    }

   private:
    struct ibv_sge sge_;
    struct ibv_recv_wr wr_;
    const uint32_t buflen_;
    const std::unique_ptr<char[]> buf_;
    const std::unique_ptr<LocalMemoryRegion> mr_;

  };

 private:
  VerbsCommon::RDMACommID id_;
  uint32_t max_inline_data_;
  uint32_t max_send_wr_;
  uint32_t max_send_sge_;
  uint32_t max_recv_sge_;
  RecvBuffer recv_buf_;

  size_t PostSend(enum ibv_wr_opcode opcode, void *buf, size_t len, uint32_t lkey, void *raddr, uint32_t rkey) const {
    struct ibv_sge sge;
    struct ibv_send_wr wr, *bad_wr;

    std::memset(&sge, 0, sizeof(sge));
    sge.addr = reinterpret_cast<uintptr_t>(buf);
    sge.length = static_cast<uint32_t>(len);
    sge.lkey = lkey;

    std::memset(&wr, 0, sizeof(wr));
    wr.sg_list = &sge;
    wr.num_sge = 1;
    wr.opcode = opcode;
    if (opcode == IBV_WR_RDMA_READ || opcode == IBV_WR_RDMA_WRITE) {
      wr.wr.rdma.rkey = rkey;
      wr.wr.rdma.remote_addr = reinterpret_cast<uintptr_t>(raddr);
    }
    if ((opcode == IBV_WR_SEND || opcode == IBV_WR_RDMA_WRITE) && len <= max_inline_data_) {
      wr.send_flags |= IBV_SEND_INLINE;
    }

    if (ibv_post_send(id_->qp, &wr, &bad_wr)) {
      // error
      return 0;
    }

    return PollCQ(id_->send_cq) ? len : 0;
  }

  bool PollCQ(struct ibv_cq *cq) const {
    int ret = 0;
    struct ibv_wc wc;

    // poll
    while ((ret = ibv_poll_cq(cq, 1, &wc)) == 0);

    return (ret < 0) ? false : (wc.status == IBV_WC_SUCCESS);
  }
};

} // namespace verbs
} // namespace rnetlib

#endif // RNETLIB_VERBS_VERBS_CHANNEL_H_
