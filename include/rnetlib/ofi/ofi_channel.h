#ifndef RNETLIB_OFI_OFI_CHANNEL_H_
#define RNETLIB_OFI_OFI_CHANNEL_H_

#include <cstring>
#include <vector>

#include "rnetlib/channel.h"
#include "rnetlib/eager_buffer.h"
#include "rnetlib/ofi/ofi_endpoint.h"

namespace rnetlib {
namespace ofi {

class OFIChannel : public Channel {
 public:
  OFIChannel(OFIEndpoint &ep, fi_addr_t peer_addr, uint64_t peer_desc)
      : ep_(ep), peer_addr_(peer_addr), peer_desc_(peer_desc), recv_buf_(*this) {
    ep_.RegisterContext(&tx_ctx_);
    ep_.RegisterContext(&rx_ctx_);
    ep_.PostRecv(recv_buf_.GetAddr(), EAGER_THRESHOLD, recv_buf_.GetLKey(), peer_addr_, TAG_MSG, &rx_ctx_);
  }

  ~OFIChannel() override {
    ep_.DeregisterContext(&tx_ctx_);
    ep_.DeregisterContext(&rx_ctx_);
    ep_.RemoveAddr(&peer_addr_);
  }

  uint64_t GetDesc() const override { return peer_desc_; }

  bool SetNonBlocking(bool non_blocking) override { return true; }

  size_t Send(void *buf, size_t len) override { return Send(RegisterMemoryRegion(buf, len, MR_LOCAL_READ)); }

  size_t Recv(void *buf, size_t len) override { return Recv(RegisterMemoryRegion(buf, len, MR_LOCAL_WRITE)); }

  size_t Send(const LocalMemoryRegion::ptr &lmr) override { return SendV(&lmr, 1); }

  size_t Recv(const LocalMemoryRegion::ptr &lmr) override { return RecvV(&lmr, 1); }

  size_t ISend(void *buf, size_t len, EventLoop &evloop) override {
    // FIXME: implement this
    return 0;
  }

  size_t IRecv(void *buf, size_t len, EventLoop &evloop) override {
    // FIXME: implement this
    return 0;
  }

  size_t SendV(const LocalMemoryRegion::ptr *lmr, size_t lmrcnt) override {
    assert(OFI_CTX_WR(&tx_ctx_) == 0);
    size_t sent_len = 0;
    std::vector<struct iovec> iov;
    std::vector<void *> desc;
    iov.reserve(lmrcnt);
    desc.reserve(lmrcnt);

    for (size_t i = 0; i < lmrcnt; i++) {
      auto len = lmr[i]->GetLength();
      if (len == 0) {
        continue;
      }
      auto addr = lmr[i]->GetAddr();
      auto lkey = lmr[i]->GetLKey();

      if (sent_len == 0) {
        // this is the very first part of the transfer, send it to the pre-allocated ReceiveBuffer.
        auto sending_len = (len > EAGER_THRESHOLD) ? EAGER_THRESHOLD : len;
        ep_.PostSend(addr, sending_len, lkey, peer_addr_, TAG_MSG, &tx_ctx_);
        if (ep_.PollTxCQ(OFI_CTX_WR(&tx_ctx_), &tx_ctx_) != 1) {
          return 0;
        }
        sent_len += sending_len;

        len -= sending_len;
        if (len == 0) {
          continue;
        }
        addr = reinterpret_cast<char *>(addr) + sending_len;
      }

      iov.push_back({addr, len});
      desc.push_back(lkey);
      sent_len += len;
    }

    if (ep_.PostSend(iov.data(), desc.data(), iov.size(), peer_addr_, &tx_ctx_)) {
      return 0;
    }

    ep_.PollTxCQ(OFI_CTX_WR(&tx_ctx_), &tx_ctx_);
    return (OFI_CTX_WR(&tx_ctx_) == 0) ? sent_len : 0;
  }

  size_t RecvV(const LocalMemoryRegion::ptr *lmr, size_t lmrcnt) override {
    assert(OFI_CTX_WR(&rx_ctx_) == 1);
    size_t recvd_len = 0;
    void *head_addr = nullptr;
    size_t head_len = 0;
    std::vector<struct iovec> iov;
    std::vector<void *> desc;
    iov.reserve(lmrcnt);
    desc.reserve(lmrcnt);

    for (auto i = 0; i < lmrcnt; i++) {
      auto len = lmr[i]->GetLength();
      if (len == 0) {
        continue;
      }
      auto addr = lmr[i]->GetAddr();
      auto lkey = lmr[i]->GetLKey();

      if (recvd_len == 0) {
        // this is the very first part of the transfer, receive it with the pre-allocated ReceiveBuffer.
        head_addr = addr;
        if (len > EAGER_THRESHOLD) {
          // the first SGE won't fit in the the pre-allocated ReceiveBuffer
          head_len = EAGER_THRESHOLD;
          len -= EAGER_THRESHOLD;
          addr = reinterpret_cast<char *>(addr) + EAGER_THRESHOLD;
          recvd_len += EAGER_THRESHOLD;
        } else {
          head_len = len;
          recvd_len += len;
          continue;
        }
      }

      iov.push_back({addr, len});
      desc.push_back(lkey);
      recvd_len += len;
    }

    ep_.PostRecv(iov.data(), desc.data(), iov.size(), peer_addr_, &rx_ctx_);

    // refill a recv request for a header.
    ep_.PostRecv(recv_buf_.GetAddr(), EAGER_THRESHOLD, recv_buf_.GetLKey(), peer_addr_, TAG_MSG, &rx_ctx_);

    ep_.PollRxCQ(OFI_CTX_WR(&rx_ctx_) - 1, &rx_ctx_);
    if (OFI_CTX_WR(&rx_ctx_) != 1) {
      // error
      return 0;
    }

    // copy the received header part to the user buffer.
    recv_buf_.Read(head_addr, head_len);

    return recvd_len;
  }

  size_t ISendV(const LocalMemoryRegion::ptr *lmr, size_t lmrcnt, EventLoop &evloop) override {
    // FIXME: implement this
    return 0;
  }

  size_t IRecvV(const LocalMemoryRegion::ptr *lmr, size_t lmrcnt, EventLoop &evloop) override {
    // FIXME: implement this
    return 0;
  }

  size_t Write(void *buf, size_t len, const RemoteMemoryRegion &rmr) override {
    return Write(RegisterMemoryRegion(buf, len, MR_REMOTE_READ), rmr);
  }

  size_t Read(void *buf, size_t len, const RemoteMemoryRegion &rmr) override {
    return Read(RegisterMemoryRegion(buf, len, MR_LOCAL_WRITE), rmr);
  }

  size_t Write(const LocalMemoryRegion::ptr &lmr, const RemoteMemoryRegion &rmr) override {
    return WriteV(&lmr, &rmr, 1);
  }

  size_t Read(const LocalMemoryRegion::ptr &lmr, const RemoteMemoryRegion &rmr) override {
    return ReadV(&lmr, &rmr, 1);
  }

  size_t WriteV(const LocalMemoryRegion::ptr *lmr, const RemoteMemoryRegion *rmr, size_t cnt) override {
    assert(OFI_CTX_WR(&tx_ctx_) == 0);
    size_t len, total_len = 0;
    struct fi_msg_rma msg;
    std::vector<struct iovec> iov;
    std::vector<struct fi_rma_iov> rma_iov;
    std::vector<void *> desc;
    iov.reserve(cnt);
    rma_iov.reserve(cnt);
    desc.reserve(cnt);

    for (size_t i = 0; i < cnt; i++) {
      len = lmr[i]->GetLength();
      if (len > 0) {
        assert(len == rmr[i].length);
        iov.push_back({lmr[i]->GetAddr(), len});
        rma_iov.push_back({0, rmr[i].length, rmr[i].rkey});
        desc.push_back(lmr[i]->GetLKey());
        total_len += len;
      }
    }

    msg.addr = peer_addr_;
    msg.msg_iov = iov.data();
    msg.iov_count = iov.size();
    msg.rma_iov = rma_iov.data();
    msg.rma_iov_count = rma_iov.size();

    ep_.PostWrite(&msg, &tx_ctx_);
    ep_.PollTxCQ(OFI_CTX_WR(&tx_ctx_), &tx_ctx_);
    return (OFI_CTX_WR(&tx_ctx_) == 0) ? total_len : 0;
  }

  size_t ReadV(const LocalMemoryRegion::ptr *lmr, const RemoteMemoryRegion *rmr, size_t cnt) override {
    assert(OFI_CTX_WR(&tx_ctx_) == 0);
    size_t len, total_len = 0;
    struct fi_msg_rma msg;
    std::vector<struct iovec> iov;
    std::vector<struct fi_rma_iov> rma_iov;
    std::vector<void *> desc;
    iov.reserve(cnt);
    rma_iov.reserve(cnt);
    desc.reserve(cnt);

    for (size_t i = 0; i < cnt; i++) {
      len = lmr[i]->GetLength();
      if (len > 0) {
        assert(len == rmr[i].length);
        iov.push_back({lmr[i]->GetAddr(), len});
        rma_iov.push_back({0, rmr[i].length, rmr[i].rkey});
        desc.push_back(lmr[i]->GetLKey());
        total_len += len;
      }
    }

    msg.addr = peer_addr_;
    msg.msg_iov = iov.data();
    msg.iov_count = iov.size();
    msg.rma_iov = rma_iov.data();
    msg.rma_iov_count = rma_iov.size();

    ep_.PostRead(&msg, &tx_ctx_);
    ep_.PollTxCQ(OFI_CTX_WR(&tx_ctx_), &tx_ctx_);
    return (OFI_CTX_WR(&tx_ctx_) == 0) ? total_len : 0;
  }

  LocalMemoryRegion::ptr RegisterMemoryRegion(void *addr, size_t len, int type) const override {
    return ep_.RegisterMemoryRegion(addr, len, type);
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

 private:
  OFIEndpoint &ep_;
  fi_addr_t peer_addr_;
  uint64_t peer_desc_;
  struct ofi_context tx_ctx_;
  struct ofi_context rx_ctx_;
  // pre-allocated buffer for eager send/recv
  EagerBuffer recv_buf_;
};

} // namespace ofi
} // namespace rnetlib

#endif // RNETLIB_OFI_OFI_CHANNEL_H_
