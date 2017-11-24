#ifndef RNETLIB_OFI_OFI_ENDPOINT_H_
#define RNETLIB_OFI_OFI_ENDPOINT_H_

#include <rdma/fabric.h>
#include "rdma/fi_cm.h"
#include <rdma/fi_domain.h>
#include <rdma/fi_endpoint.h>
#include <rdma/fi_rma.h>
#include <rdma/fi_tagged.h>

#include <iostream>
#include <string>

#include "rnetlib/ofi/ofi_local_memory_region.h"

#define OFI_VERSION FI_VERSION(1, 5)

#define OFI_PRINTERR(tag, err)                                     \
  do {                                                             \
    std::cerr << __FILE__ << ":" << __LINE__ << " OFI_"#tag": "    \
              << fi_strerror(static_cast<int>(-err)) << std::endl; \
  } while(0)                                                       \

#define OFI_CALL(func, str)                                        \
  do {                                                             \
    auto ret = func;                                               \
    if (ret < 0) {                                                 \
      OFI_PRINTERR(str, ret);                                      \
      return;                                                      \
    }                                                              \
  } while (0)                                                      \

#define OFI_POST(post_func, comp_func, cnt)                               \
  do {                                                                    \
    while (true) {                                                        \
      ssize_t ret = post_func;                                            \
      if (ret == 0) {                                                     \
        (cnt)++;                                                          \
        break;                                                            \
      } else if (ret != -FI_EAGAIN) {                                     \
        OFI_PRINTERR(post, ret);                                          \
        return 0;                                                         \
      } else if (info_->domain_attr->data_progress != FI_PROGRESS_AUTO) { \
        (cnt) -= comp_func(1);                                            \
      }                                                                   \
    }                                                                     \
  } while (0)                                                             \

namespace rnetlib {
namespace ofi {

struct ofi_addrinfo {
  union sockaddr_any {
    struct sockaddr sa;
    struct sockaddr_in sin;
    struct sockaddr_in6 sin6;
    struct sockaddr_storage ss;
  } addr;
  size_t addrlen;
};

class OFIEndpoint {
 public:
  static OFIEndpoint &GetInstance(const std::string &addr, uint16_t port, uint64_t flags) {
    static OFIEndpoint ep(addr.empty() ? nullptr : addr.c_str(), port, flags);
    return ep;
  }

  virtual ~OFIEndpoint() = default;

  LocalMemoryRegion::ptr RegisterMemoryRegion(void *buf, size_t len, int type) {
    static uint64_t requested_key = 0;

    // FIXME: skip local memory registration when FI_MR_LOCAL is off
    uint64_t access = (FI_SEND | FI_RECV | FI_WRITE | FI_READ);
    if (type & MR_REMOTE_READ) {
      access |= FI_REMOTE_READ;
    }
    if (type & MR_REMOTE_WRITE) {
      access |= FI_REMOTE_WRITE;
    }

    struct fid_mr *tmp_mr = nullptr;
    auto ret = fi_mr_reg(domain_.get(), buf, len, access, 0, requested_key++, 0, &tmp_mr, nullptr);
    if (ret) {
      OFI_PRINTERR(mr_reg, ret);
      return nullptr;
    }

    return LocalMemoryRegion::ptr(new OFILocalMemoryRegion(tmp_mr, buf, len));
  }

  ssize_t PostSend(const void *buf, size_t len, void *desc, fi_addr_t dst_addr, uint64_t tag, size_t *cnt) {
    if (tag) {
      OFI_POST(fi_tsend(ep_.get(), buf, len, desc, dst_addr, tag, &tx_ctx_), PollTxCQ, *cnt);
    } else {
      OFI_POST(fi_send(ep_.get(), buf, len, desc, dst_addr, &tx_ctx_), PollTxCQ, *cnt);
    }

    return 0;
  }

  ssize_t PostSend(struct iovec *iov, void **desc, size_t iovcnt, fi_addr_t dst_addr, size_t *cnt) {
    size_t offset = 0;

    while (offset < iovcnt) {
      auto num_iov = ((iovcnt - offset) > max_msg_iov) ? max_msg_iov : (iovcnt - offset);
      OFI_POST(fi_sendv(ep_.get(), iov + offset, desc, num_iov, dst_addr, &tx_ctx_), PollTxCQ, *cnt);
      offset += num_iov;
    }

    return 0;
  }

  ssize_t PostRecv(void *buf, size_t len, void *desc, fi_addr_t src_addr, uint64_t tag, size_t *cnt) {
    if (tag) {
      OFI_POST(fi_trecv(ep_.get(), buf, len, desc, src_addr, tag, 0, &rx_ctx_), PollRxCQ, *cnt);
    } else {
      OFI_POST(fi_recv(ep_.get(), buf, len, desc, src_addr, &rx_ctx_), PollRxCQ, *cnt);
    }

    return 0;
  }

  ssize_t PostRecv(struct iovec *iov, void **desc, size_t iovcnt, fi_addr_t dst_addr, size_t *cnt) {
    size_t offset = 0;

    while (offset < iovcnt) {
      auto num_iov = ((iovcnt - offset) > max_msg_iov) ? max_msg_iov : (iovcnt - offset);
      OFI_POST(fi_recvv(ep_.get(), iov + offset, desc, num_iov, dst_addr, &rx_ctx_), PollRxCQ, *cnt);
      offset += num_iov;
    }

    return 0;
  }

  ssize_t PostWrite(struct fi_msg_rma *msg, size_t *cnt) {
    size_t offset = 0, iovcnt = msg->iov_count;
    auto msg_iov_head = msg->msg_iov;
    auto rma_iov_head = msg->rma_iov;
    auto desc_head = msg->desc;
    msg->context = &tx_ctx_;

    while (offset < iovcnt) {
      auto num_iov = ((iovcnt - offset) > max_rma_iov) ? max_rma_iov : (iovcnt - offset);
      msg->msg_iov = msg_iov_head + offset;
      msg->iov_count = num_iov;
      msg->rma_iov = rma_iov_head + offset;
      msg->rma_iov_count = num_iov;
      msg->desc = desc_head + offset;
      OFI_POST(fi_writemsg(ep_.get(), msg, 0), PollTxCQ, *cnt);
      offset += num_iov;
    }

    return 0;
  }

  ssize_t PostRead(struct fi_msg_rma *msg, size_t *cnt) {
    size_t offset = 0, iovcnt = msg->iov_count;
    auto msg_iov_head = msg->msg_iov;
    auto rma_iov_head = msg->rma_iov;
    auto desc_head = msg->desc;
    msg->context = &tx_ctx_;

    while (offset < iovcnt) {
      auto num_iov = ((iovcnt - offset) > max_rma_iov) ? max_rma_iov : (iovcnt - offset);
      msg->msg_iov = msg_iov_head + offset;
      msg->iov_count = num_iov;
      msg->rma_iov = rma_iov_head + offset;
      msg->rma_iov_count = num_iov;
      msg->desc = desc_head + offset;
      OFI_POST(fi_readmsg(ep_.get(), msg, 0), PollTxCQ, *cnt);
      offset += num_iov;
    }

    return 0;
  }

  size_t PollTxCQ(size_t count) { return PollCQ(tx_cq_.get(), count, &tx_ctx_); }

  size_t PollRxCQ(size_t count) { return PollCQ(rx_cq_.get(), count, &rx_ctx_); }

  void InsertAddr(const void *addr, size_t count, fi_addr_t *fi_addr) {
    int ret = fi_av_insert(av_.get(), addr, 1, fi_addr, 0, nullptr);
    if (ret < 0) {
      OFI_PRINTERR(av_insert, ret);
    } else if (ret != count) {
      // error
    }
  }

  void RemoveAddr(fi_addr_t *fi_addr, size_t count) {
    int ret = fi_av_remove(av_.get(), fi_addr, count, 0);
    if (ret < 0) {
      OFI_PRINTERR(av_insert, ret);
    } else if (ret != count) {
      // error
    }
  }

  void GetAddrInfo(struct ofi_addrinfo *addrinfo) {
    assert(addrinfo);
    addrinfo->addrlen = sizeof(addrinfo->addr);
    OFI_CALL(fi_getname(&ep_->fid, &addrinfo->addr, &addrinfo->addrlen), getname);
  }

  void *GetDestAddrPtr() { return info_->dest_addr; }

  uint16_t GetBindPort() {
    char servbuf[BUFSIZ];

    struct ofi_addrinfo addrinfo;
    GetAddrInfo(&addrinfo);
    getnameinfo(&addrinfo.addr.sa, addrinfo.addrlen, nullptr, 0, servbuf, BUFSIZ, NI_NUMERICHOST | NI_NUMERICSERV);
    return static_cast<uint16_t>(std::stoul(servbuf));
  }

 private:
  template <typename T>
  using ofi_ptr = std::unique_ptr<T, std::function<void(T *)>>;

  template <typename T>
  static void fid_deleter(T *fd) { OFI_CALL(fi_close(reinterpret_cast<struct fid *>(fd)), fid_close); }

  ofi_ptr<struct fi_info> info_;
  ofi_ptr<struct fid_fabric> fabric_;
  ofi_ptr<struct fid_domain> domain_;
  ofi_ptr<struct fid_cq> tx_cq_;
  ofi_ptr<struct fid_cq> rx_cq_;
  ofi_ptr<struct fid_av> av_;
  ofi_ptr<struct fid_ep> ep_;
  struct fi_context tx_ctx_;
  struct fi_context rx_ctx_;
  size_t max_msg_iov;
  size_t max_rma_iov;

  OFIEndpoint(const char *addr, uint16_t port, uint64_t flags)
      : info_(nullptr, fi_freeinfo), fabric_(nullptr, fid_deleter<struct fid_fabric>),
        domain_(nullptr, fid_deleter<struct fid_domain>), tx_cq_(nullptr, fid_deleter<struct fid_cq>),
        rx_cq_(nullptr, fid_deleter<struct fid_cq>), av_(nullptr, fid_deleter<struct fid_av>),
        ep_(nullptr, fid_deleter<struct fid_ep>) {
    ofi_ptr<struct fi_info> hints(fi_allocinfo(), fi_freeinfo);
    if (!hints) {
      return;
    }

    hints->caps = FI_MSG | FI_RMA | FI_TAGGED;
    hints->mode = FI_CONTEXT;
    hints->ep_attr->type = FI_EP_RDM;
    hints->rx_attr->caps = FI_DIRECTED_RECV;
    hints->rx_attr->msg_order = FI_ORDER_SAS;
    hints->rx_attr->op_flags = FI_COMPLETION;
    hints->tx_attr->comp_order = FI_ORDER_NONE;
    hints->tx_attr->msg_order = FI_ORDER_SAS;
    hints->tx_attr->op_flags = FI_DELIVERY_COMPLETE | FI_COMPLETION;

    // get provider information
    struct fi_info *tmp_info = nullptr;
    OFI_CALL(fi_getinfo(OFI_VERSION, addr, std::to_string(port).c_str(), flags, hints.get(), &tmp_info), getinfo);
    info_.reset(tmp_info);

    max_msg_iov = std::min(info_->tx_attr->iov_limit, info_->rx_attr->iov_limit);
    max_rma_iov = info_->tx_attr->rma_iov_limit;

    // initialize fabric with the first provider found
    struct fid_fabric *tmp_fabric = nullptr;
    OFI_CALL(fi_fabric(info_->fabric_attr, &tmp_fabric, nullptr), fabric);
    fabric_.reset(tmp_fabric);

    // initialize domain (local NIC)
    struct fid_domain *tmp_domain = nullptr;
    OFI_CALL(fi_domain(fabric_.get(), info_.get(), &tmp_domain, nullptr), domain);
    domain_.reset(tmp_domain);

    // initialize completion queues
    struct fi_cq_attr cq_attr;
    std::memset(&cq_attr, 0, sizeof(cq_attr));
    cq_attr.wait_obj = FI_WAIT_NONE;
    cq_attr.format = FI_CQ_FORMAT_CONTEXT;

    // outgoing completion queue
    cq_attr.size = info_->tx_attr->size;
    struct fid_cq *tmp_txcq = nullptr;
    OFI_CALL(fi_cq_open(domain_.get(), &cq_attr, &tmp_txcq, nullptr), cq_open);
    tx_cq_.reset(tmp_txcq);

    // incoming completion queue
    cq_attr.size = info_->rx_attr->size;
    struct fid_cq *tmp_rxcq = nullptr;
    OFI_CALL(fi_cq_open(domain_.get(), &cq_attr, &tmp_rxcq, nullptr), cq_open);
    rx_cq_.reset(tmp_rxcq);

    // TODO: retrieve completion with fi_cntr?

    // open address vector
    struct fi_av_attr av_attr;
    std::memset(&av_attr, 0, sizeof(av_attr));
    av_attr.type = FI_AV_MAP;
    av_attr.count = 32; // FIXME: actual # of procs.?

    struct fid_av *tmp_av = nullptr;
    OFI_CALL(fi_av_open(domain_.get(), &av_attr, &tmp_av, nullptr), av_open);
    av_.reset(tmp_av);

    // open endpoint
    struct fid_ep *tmp_ep = nullptr;
    OFI_CALL(fi_endpoint(domain_.get(), info_.get(), &tmp_ep, nullptr), endpoint);
    ep_.reset(tmp_ep);

    // bind resources to the endpoint
    OFI_CALL(fi_ep_bind(ep_.get(), &av_->fid, 0), ep_bind);
    OFI_CALL(fi_ep_bind(ep_.get(), &tx_cq_->fid, FI_TRANSMIT), ep_bind);
    OFI_CALL(fi_ep_bind(ep_.get(), &rx_cq_->fid, FI_RECV), ep_bind);

    // enable endpoint
    OFI_CALL(fi_enable(ep_.get()), ep_enable);
  }

  size_t PollCQ(struct fid_cq *cq, size_t count, void *ctx) {
    size_t polled = 0;
    ssize_t ret = 0;
    struct fi_cq_err_entry comp;

    while (count - polled) {
      // FIXME: implement timeout
      ret = fi_cq_read(cq, &comp, 1);
      if (ret > 0) {
        assert(comp.op_context == ctx);
        polled++;
      } else if (ret < 0 && ret == -FI_EAVAIL) {
        ret = fi_cq_readerr(cq, &comp, 0);
        if (ret < 0) {
          OFI_PRINTERR(cq_readerr, ret);
          break;
        }
        std::cerr << fi_cq_strerror(cq, comp.prov_errno, comp.err_data, nullptr, 0) << std::endl;
        polled++;
      } else if (ret < 0 && ret != -FI_EAGAIN) {
        OFI_PRINTERR(cq_read, ret);
        break;
      }
    }

    return polled;
  }
};

} // namespace ofi
} // namespace rnetlib

#endif // RNETLIB_OFI_OFI_ENDPOINT_H_
