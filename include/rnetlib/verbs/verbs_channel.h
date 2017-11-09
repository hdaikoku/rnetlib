#ifndef RNETLIB_VERBS_VERBS_CHANNEL_H_
#define RNETLIB_VERBS_VERBS_CHANNEL_H_

#include <fcntl.h>
#include <poll.h>
#include <infiniband/verbs.h>

#include <cassert>
#include <cstring>

#include "rnetlib/channel.h"
#include "rnetlib/verbs/verbs_common.h"
#include "rnetlib/verbs/verbs_local_memory_region.h"

#define IBV_RCVBUF 32

namespace rnetlib {
namespace verbs {

class VerbsChannel : public Channel {
 public:
  explicit VerbsChannel(VerbsCommon::RDMACommID id)
      : id_(std::move(id)), num_recv_wr_(0), num_send_wr_(0), recv_buf_(IBV_RCVBUF, id_->pd) {
    struct ibv_qp_attr attr;
    struct ibv_qp_init_attr init_attr;

    ibv_query_qp(id_->qp, &attr, 0, &init_attr);
    max_inline_data_ = attr.cap.max_inline_data;
    // max_recv_wr has to be at least twice as big as max_send_wr
    // so that Recv Queue constantly keeps "max_send_wr" requests for incoming requests.
    max_recv_wr_ = std::min(attr.cap.max_send_wr, attr.cap.max_recv_wr);
    max_send_wr_ = max_recv_wr_ - 1;
    max_recv_sge_ = max_send_sge_ = std::min(attr.cap.max_send_sge, attr.cap.max_recv_sge);

    struct ibv_port_attr port_attr;
    ibv_query_port(id_->verbs, 1, &port_attr);
    max_msg_sz_ = port_attr.max_msg_sz;

    PostRecv(recv_buf_.GetSGEPtr(), 1);
  }

  virtual ~VerbsChannel() {
    if (id_) {
      rdma_disconnect(id_.get());
    }
  }

  bool SetNonBlocking(bool non_blocking) override {
    return true;
  }

  size_t Send(void *buf, size_t len) override {
    return Send(RegisterMemory(buf, len, MR_LOCAL_READ));
  }

  size_t Recv(void *buf, size_t len) override {
    return Recv(RegisterMemory(buf, len, MR_LOCAL_WRITE));
  }

  size_t Send(const LocalMemoryRegion::ptr &lmr) override {
    return SendV(&lmr, 1);
  }

  size_t Recv(const LocalMemoryRegion::ptr &lmr) override {
    return RecvV(&lmr, 1);
  }

  size_t ISend(void *buf, size_t len, EventLoop &evloop) override {
    // TODO: implement this.
    return 0;
  }

  size_t IRecv(void *buf, size_t len, EventLoop &evloop) override {
    // TODO: implement this.
    return 0;
  }

  size_t SendV(const LocalMemoryRegion::ptr *lmr, size_t lmrcnt) override {
    assert(num_send_wr_ == 0);

    size_t sent_len = 0;
    std::vector<struct ibv_sge> sges;
    sges.reserve(lmrcnt);

    for (size_t i = 0; i < lmrcnt; i++) {
      auto len = lmr[i]->GetLength();
      if (len == 0) {
        continue;
      }
      auto addr = lmr[i]->GetAddr();
      auto lkey = lmr[i]->GetLKey();

      if (sent_len == 0) {
        // this is the very first part of the transfer, send it to the pre-allocated ReceiveBuffer.
        auto sending_len = (len > IBV_RCVBUF) ? IBV_RCVBUF : len;
        struct ibv_sge sge = {reinterpret_cast<uintptr_t>(addr), sending_len, lkey};
        if (PostSend(IBV_WR_SEND, &sge, 1, nullptr, 0) != 1) {
          // error
          return 0;
        }
        if (!PollSendCQ(1)) {
          return 0;
        }
        sent_len += sending_len;

        len -= sending_len;
        if (len == 0) {
          continue;
        }
        addr = addr + sending_len;
      }

      sges.push_back({reinterpret_cast<uintptr_t>(addr), len, lkey});
      sent_len += len;
    }

    auto num_sges = static_cast<int>(sges.size());
    if (PostSend(IBV_WR_SEND, sges.data(), num_sges, nullptr, 0) != num_sges) {
      // error
      return 0;
    }

    return PollSendCQ(num_send_wr_) ? sent_len : 0;
  }

  size_t RecvV(const LocalMemoryRegion::ptr *lmr, size_t lmrcnt) override {
    assert(num_recv_wr_ == 1);

    size_t recvd_len = 0;
    std::vector<struct ibv_sge> sges;
    sges.reserve(lmrcnt);
    void *head_sge_addr = nullptr;
    size_t head_sge_len = 0;

    for (auto i = 0; i < lmrcnt; i++) {
      auto len = lmr[i]->GetLength();
      if (len == 0) {
        continue;
      }
      auto addr = lmr[i]->GetAddr();
      auto lkey = lmr[i]->GetLKey();

      if (recvd_len == 0) {
        // this is the very first part of the transfer, receive it with the pre-allocated ReceiveBuffer.
        head_sge_addr = addr;
        if (len > IBV_RCVBUF) {
          // the first SGE won't fit in the the pre-allocated ReceiveBuffer
          head_sge_len = IBV_RCVBUF;
          len -= IBV_RCVBUF;
          addr = addr + IBV_RCVBUF;
          recvd_len += IBV_RCVBUF;
        } else {
          head_sge_len = len;
          recvd_len += len;
          continue;
        }
      }

      sges.push_back({reinterpret_cast<uintptr_t>(addr), len, lkey});
      recvd_len += len;
    }

    auto num_sges = static_cast<int>(sges.size());
    if (PostRecv(sges.data(), num_sges) != num_sges) {
      // error
      return 0;
    }

    // refill a recv request for a header.
    if (PostRecv(recv_buf_.GetSGEPtr(), 1) != 1) {
      // error
      return 0;
    }

    if (!PollRecvCQ(num_recv_wr_ - 1)) {
      // error
      return 0;
    }

    // copy the received header part to the user buffer.
    recv_buf_.Read(head_sge_addr, head_sge_len);

    return recvd_len;
  }

  size_t ISendV(const LocalMemoryRegion::ptr *lmr, size_t lmrcnt, EventLoop &evloop) override {
    // TODO: implement this.
    return 0;
  }

  size_t IRecvV(const LocalMemoryRegion::ptr *lmr, size_t lmrcnt, EventLoop &evloop) override {
    // TODO: implement this.
    return 0;
  }

  size_t Write(const LocalMemoryRegion::ptr &lmr, const RemoteMemoryRegion &rmr) override {
    return WriteV(&lmr, &rmr, 1);
  }

  size_t Read(const LocalMemoryRegion::ptr &lmr, const RemoteMemoryRegion &rmr) override {
    return ReadV(&lmr, &rmr, 1);
  }

  size_t WriteV(const LocalMemoryRegion::ptr *lmr, const RemoteMemoryRegion *rmr, size_t cnt) override {
    struct ibv_sge sge;
    size_t len, total_len = 0;

    for (size_t i = 0; i < cnt; i++) {
      len = lmr[i]->GetLength();
      if (len > 0) {
        assert(len == rmr[i].length);
        sge = {reinterpret_cast<uintptr_t>(lmr[i]->GetAddr()), len, lmr[i]->GetLKey()};
        if (PostSend(IBV_WR_RDMA_WRITE, &sge, 1, reinterpret_cast<void *>(rmr[i].addr), rmr[i].rkey) != 1) {
          // error
          break;
        }
        total_len += len;
      }
    }

    return PollSendCQ(num_send_wr_) ? total_len : 0;
  }

  size_t ReadV(const LocalMemoryRegion::ptr *lmr, const RemoteMemoryRegion *rmr, size_t cnt) override {
    struct ibv_sge sge;
    size_t len, total_len = 0;

    for (size_t i = 0; i < cnt; i++) {
      len = lmr[i]->GetLength();
      if (len > 0) {
        assert(len == rmr[i].length);
        sge = {reinterpret_cast<uintptr_t>(lmr[i]->GetAddr()), len, lmr[i]->GetLKey()};
        if (PostSend(IBV_WR_RDMA_READ, &sge, 1, reinterpret_cast<void *>(rmr[i].addr), rmr[i].rkey) != 1) {
          // error
          break;
        }
        total_len += len;
      }
    }

    return PollSendCQ(num_send_wr_) ? total_len : 0;
  }

  LocalMemoryRegion::ptr RegisterMemory(void *addr, size_t len, int type) const override {
    return VerbsLocalMemoryRegion::Register(id_->pd, addr, len, type);
  }

  void SynRemoteMemoryRegionV(const LocalMemoryRegion::ptr *lmr, size_t lmrcnt) override {
    std::vector<RemoteMemoryRegion> rmrs;
    rmrs.reserve(lmrcnt);

    for (auto i = 0; i < lmrcnt; i++) {
      rmrs.emplace_back(*lmr[i]);
    }

    Send(rmrs.data(), sizeof(RemoteMemoryRegion) * rmrs.size());
  }

  void AckRemoteMemoryRegionV(RemoteMemoryRegion *rmr, size_t rmrcnt) override {
    Recv(rmr, sizeof(RemoteMemoryRegion) * rmrcnt);
  }

  const struct rdma_cm_id *GetIDPtr() const { return id_.get(); }

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
    }

    size_t Read(void *addr, size_t len) const {
      auto cpylen = (len > buflen_) ? buflen_ : len;
      std::memcpy(addr, buf_.get(), cpylen);

      return cpylen;
    }

    struct ibv_sge *GetSGEPtr() { return &sge_; }

   private:
    struct ibv_sge sge_;
    const uint32_t buflen_;
    const std::unique_ptr<char[]> buf_;
    const LocalMemoryRegion::ptr mr_;
  };

 private:
  VerbsCommon::RDMACommID id_;
  uint32_t max_inline_data_;
  uint32_t max_recv_wr_;
  uint32_t max_send_wr_;
  uint32_t max_recv_sge_;
  uint32_t max_send_sge_;
  uint32_t num_recv_wr_;
  uint32_t num_send_wr_;
  uint32_t max_msg_sz_;
  RecvBuffer recv_buf_;

  int PostSend(enum ibv_wr_opcode opcode, struct ibv_sge *sg_list, int num_sges, void *raddr, uint32_t rkey) {
    int offset = 0;
    struct ibv_send_wr wr, *bad_wr;

    while (offset < num_sges) {
      int num_sending_sges = (num_sges - offset) > max_send_sge_ ? max_send_sge_ : (num_sges - offset);

      std::memset(&wr, 0, sizeof(wr));
      wr.sg_list = sg_list + offset;
      wr.opcode = opcode;

      uint32_t sending_len = 0, last_sge_rem = 0;
      for (int i = 0; i < num_sending_sges; i++) {
        wr.num_sge++;
        if (sending_len + sg_list[offset + i].length > max_msg_sz_) {
          last_sge_rem = sg_list[offset + i].length - (max_msg_sz_ - sending_len);
          sg_list[offset + i].length -= last_sge_rem;
          sending_len = max_msg_sz_;
          num_sending_sges = i;
          break;
        } else {
          sending_len += sg_list[offset + i].length;
        }
      }
      offset += num_sending_sges;

      if ((opcode == IBV_WR_SEND || opcode == IBV_WR_RDMA_WRITE) && sending_len <= max_inline_data_) {
        wr.send_flags |= IBV_SEND_INLINE;
      }
      if (opcode == IBV_WR_RDMA_READ || opcode == IBV_WR_RDMA_WRITE) {
        wr.wr.rdma.rkey = rkey;
        wr.wr.rdma.remote_addr = reinterpret_cast<uintptr_t>(raddr);
      }

      if (num_send_wr_ == max_send_wr_) {
        // FIXME: this might block
        if (!PollSendCQ(1)) {
          // error
          break;
        }
      }

      if (ibv_post_send(id_->qp, &wr, &bad_wr)) {
        // error
        break;
      }
      num_send_wr_++;

      if (last_sge_rem) {
        sg_list[offset].addr += sg_list[offset].length;
        raddr += sg_list[offset].length;
        sg_list[offset].length = last_sge_rem;
      }
    }

    return offset;
  }

  int PostRecv(struct ibv_sge *sg_list, int num_sges) {
    int offset = 0;
    struct ibv_recv_wr wr, *bad_wr;

    while (offset < num_sges) {
      int num_recving_sges = (num_sges - offset) > max_recv_sge_ ? max_recv_sge_ : (num_sges - offset);

      std::memset(&wr, 0, sizeof(wr));
      wr.sg_list = sg_list + offset;

      uint32_t recving_len = 0, last_sge_rem = 0;
      for (int i = 0; i < num_recving_sges; i++) {
        wr.num_sge++;
        if (recving_len + sg_list[offset + i].length > max_msg_sz_) {
          last_sge_rem = sg_list[offset + i].length - (max_msg_sz_ - recving_len);
          sg_list[offset + i].length -= last_sge_rem;
          recving_len = max_msg_sz_;
          num_recving_sges = i;
          break;
        } else {
          recving_len += sg_list[offset + i].length;
        }
      }
      offset += num_recving_sges;

      if (num_recv_wr_ == max_recv_wr_) {
        // FIXME: this might block
        if (!PollRecvCQ(1)) {
          // error
          break;
        }
      }

      if (ibv_post_recv(id_->qp, &wr, &bad_wr)) {
        // error
        break;
      }
      num_recv_wr_++;

      if (last_sge_rem) {
        sg_list[offset].addr += sg_list[offset].length;
        sg_list[offset].length = last_sge_rem;
      }
    }

    return offset;
  }

  bool PollSendCQ(uint32_t num_cqes) {
    auto polled = PollCQ(id_->send_cq, num_cqes);
    num_send_wr_ -= polled;
    return (polled == num_cqes);
  }

  bool PollRecvCQ(uint32_t num_cqes) {
    auto polled = PollCQ(id_->recv_cq, num_cqes);
    num_recv_wr_ -= polled;
    return (polled == num_cqes);
  }

  uint32_t PollCQ(struct ibv_cq *cq, uint32_t num_cqes) {
    int ret = 0;
    uint32_t num_polled = 0;
    struct ibv_wc wc;

    for (uint32_t i = 0; i < num_cqes; i++) {
      // poll
      while ((ret = ibv_poll_cq(cq, 1, &wc)) == 0);

      if (ret < 0 || wc.status != IBV_WC_SUCCESS) {
        // error
        continue;
      }
      num_polled++;
    }

    return num_polled;
  }
};

} // namespace verbs
} // namespace rnetlib

#endif // RNETLIB_VERBS_VERBS_CHANNEL_H_
