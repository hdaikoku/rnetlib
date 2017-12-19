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
  OFIChannel(OFIEndpoint &ep, fi_addr_t peer_addr, uint64_t peer_desc, uint64_t src_tag)
      : ep_(ep), peer_addr_(peer_addr), peer_desc_(peer_desc), src_tag_(src_tag), dst_tag_(0) {
    std::memset(&tx_req_, 0, sizeof(tx_req_));
    std::memset(&rx_req_, 0, sizeof(rx_req_));
  }

  ~OFIChannel() override { ep_.RemoveAddr(&peer_addr_); }

  uint64_t GetDesc() const override { return peer_desc_; }

  size_t Send(void *buf, size_t len) override { return Send(RegisterMemoryRegion(buf, len, MR_LOCAL_READ)); }

  size_t Recv(void *buf, size_t len) override { return Recv(RegisterMemoryRegion(buf, len, MR_LOCAL_WRITE)); }

  size_t Send(const LocalMemoryRegion::ptr &lmr) override { return SendV(&lmr, 1); }

  size_t Recv(const LocalMemoryRegion::ptr &lmr) override { return RecvV(&lmr, 1); }

  size_t ISend(void *buf, size_t len, const EventLoop::ptr &evloop) override {
    // FIXME: implement this
    return 0;
  }

  size_t IRecv(void *buf, size_t len, const EventLoop::ptr &evloop) override {
    // FIXME: implement this
    return 0;
  }

  size_t SendV(const LocalMemoryRegion::ptr *lmr, size_t lmrcnt) override {
    assert(tx_req_.req == 0);
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

      iov.push_back({lmr[i]->GetAddr(), len});
      desc.push_back(lmr[i]->GetLKey());
      sent_len += len;
    }

    if (ep_.PostSend(iov.data(), desc.data(), iov.size(), peer_addr_, dst_tag_, &tx_req_)) {
      return 0;
    }

    ep_.PollTxCQ(tx_req_.req, &tx_req_);
    return (tx_req_.req == 0) ? sent_len : 0;
  }

  size_t RecvV(const LocalMemoryRegion::ptr *lmr, size_t lmrcnt) override {
    assert(rx_req_.req == 0);
    size_t recvd_len = 0;
    std::vector<struct iovec> iov;
    std::vector<void *> desc;
    iov.reserve(lmrcnt);
    desc.reserve(lmrcnt);

    for (auto i = 0; i < lmrcnt; i++) {
      auto len = lmr[i]->GetLength();
      if (len == 0) {
        continue;
      }

      iov.push_back({lmr[i]->GetAddr(), len});
      desc.push_back(lmr[i]->GetLKey());
      recvd_len += len;
    }

    ep_.PostRecv(iov.data(), desc.data(), iov.size(), src_tag_, &rx_req_);

    ep_.PollRxCQ(rx_req_.req, &rx_req_);
    return (rx_req_.req == 0) ? recvd_len : 0;
  }

  size_t ISendV(const LocalMemoryRegion::ptr *lmr, size_t lmrcnt, const EventLoop::ptr &evloop) override {
    // FIXME: implement this
    return 0;
  }

  size_t IRecvV(const LocalMemoryRegion::ptr *lmr, size_t lmrcnt, const EventLoop::ptr &evloop) override {
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
    assert(tx_req_.req == 0);
    size_t len, total_len = 0;
    struct fi_msg_rma msg;
    std::memset(&msg, 0, sizeof(msg));
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
    msg.desc = desc.data();

    ep_.PostWrite(&msg, &tx_req_);
    ep_.PollTxCQ(tx_req_.req, &tx_req_);
    return (tx_req_.req == 0) ? total_len : 0;
  }

  size_t ReadV(const LocalMemoryRegion::ptr *lmr, const RemoteMemoryRegion *rmr, size_t cnt) override {
    assert(tx_req_.req == 0);
    size_t len, total_len = 0;
    struct fi_msg_rma msg;
    std::memset(&msg, 0, sizeof(msg));
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
    msg.desc = desc.data();

    ep_.PostRead(&msg, &tx_req_);
    ep_.PollTxCQ(tx_req_.req, &tx_req_);
    return (tx_req_.req == 0) ? total_len : 0;
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

  void SetDestTag(uint64_t dst_tag) { dst_tag_ = dst_tag; }

 private:
  OFIEndpoint &ep_;
  fi_addr_t peer_addr_;
  uint64_t peer_desc_;
  uint64_t src_tag_;
  uint64_t dst_tag_;
  struct ofi_req tx_req_;
  struct ofi_req rx_req_;
};

} // namespace ofi
} // namespace rnetlib

#endif // RNETLIB_OFI_OFI_CHANNEL_H_
