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

#define OFI_TAG_PROTO_BITS  (2)
#define OFI_TAG_SOURCE_BITS (32)
#define OFI_TAG_PROTO_MASK  (0x300000000ULL)
#define OFI_TAG_SOURCE_MASK (0x0FFFFFFFFULL)

#define OFI_CTX_REQ(ctx) ((ctx)->req->req)
#define OFI_CTX_COMP(ctx) ((ctx)->req->comp)

#define OFI_CTX_NEW(ctx, req)  \
  do {                         \
    (ctx) = new ofi_context;   \
    (ctx)->req = req;          \
  } while (0)                  \

#define OFI_CTX_FREE(ctx) \
  do {                    \
    if (ctx)  {           \
      delete (ctx);       \
      (ctx) = nullptr;      \
    }                     \
  } while (0)             \

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

#define OFI_POST(post_func, comp_func, ctx)                               \
  do {                                                                    \
    while (true) {                                                        \
      ssize_t ret = post_func;                                            \
      if (ret == 0) {                                                     \
        OFI_CTX_REQ(ctx)++;                                               \
        break;                                                            \
      } else if (ret != -FI_EAGAIN) {                                     \
        OFI_PRINTERR(post, ret);                                          \
        OFI_CTX_FREE(ctx);                                                \
        return 0;                                                         \
      } else if (info_->domain_attr->data_progress != FI_PROGRESS_AUTO) { \
        comp_func(1, (ctx)->req);                                         \
      }                                                                   \
    }                                                                     \
  } while (0)                                                             \

namespace rnetlib {
namespace ofi {

enum TagProto : uint64_t {
  TAG_PROTO_MSG = 0,
  TAG_PROTO_CTR
};

struct ofi_addrinfo {
  char addr[FI_NAME_MAX];
  size_t addrlen;
  uint64_t desc;
  uint64_t src_tag;
};

struct ofi_req {
  uint64_t req;
  uint64_t comp;
};

class OFIEndpoint {
 public:
  static OFIEndpoint &GetInstance(const char *addr, const char *port) {
    static OFIEndpoint ep(addr, port);
    return ep;
  }

  virtual ~OFIEndpoint() = default;

  LocalMemoryRegion::ptr RegisterMemoryRegion(void *buf, size_t len, int type) {
    static uint64_t requested_key = 0;
    uint64_t access = 0;

    if (type & MR_REMOTE_READ) {
      access |= FI_REMOTE_READ;
    }
    if (type & MR_REMOTE_WRITE) {
      access |= FI_REMOTE_WRITE;
    }

    if ((info_->mode & FI_LOCAL_MR) || (info_->domain_attr->mr_mode & FI_MR_LOCAL)) {
      if (type & MR_LOCAL_READ) {
        access |= (FI_SEND | FI_WRITE);
      }
      if (type & MR_LOCAL_WRITE) {
        access |= (FI_RECV | FI_READ);
      }
    } else {
      if (access == 0) {
        return LocalMemoryRegion::ptr(new OFILocalMemoryRegion(nullptr, buf, len));
      }
    }

    struct fid_mr *tmp_mr = nullptr;
    auto ret = fi_mr_reg(domain_.get(), buf, len, access, 0, requested_key++, 0, &tmp_mr, nullptr);
    if (ret) {
      OFI_PRINTERR(mr_reg, ret);
      return nullptr;
    }

    return LocalMemoryRegion::ptr(new OFILocalMemoryRegion(tmp_mr, buf, len));
  }

  ssize_t PostSend(const void *buf, size_t len, void *desc, fi_addr_t dst_addr, uint64_t tag, struct ofi_req *req) {
    struct ofi_context *ctx = nullptr;
    OFI_CTX_NEW(ctx, req);

    OFI_POST(fi_tsend(ep_.get(), buf, len, desc, dst_addr, tag, &ctx->ctx), PollTxCQ, ctx);

    return 0;
  }

  ssize_t PostSend(struct iovec *iov, void **desc, size_t cnt, fi_addr_t dst_addr, uint64_t tag, struct ofi_req *req) {
    struct ofi_context *ctx = nullptr;
    size_t offset = 0;

    while (offset < cnt) {
      auto num_iov = ((cnt - offset) > max_msg_iov_) ? max_msg_iov_ : (cnt - offset);
      OFI_CTX_NEW(ctx, req);
      OFI_POST(fi_tsendv(ep_.get(), iov + offset, desc, num_iov, dst_addr, tag, &ctx->ctx), PollTxCQ, ctx);
      offset += num_iov;
    }

    return 0;
  }

  ssize_t PostRecv(void *buf, size_t len, void *desc, uint64_t tag, struct ofi_req *req) {
    struct ofi_context *ctx = nullptr;
    OFI_CTX_NEW(ctx, req);

    OFI_POST(fi_trecv(ep_.get(), buf, len, desc, 0, tag, 0, &ctx->ctx), PollRxCQ, ctx);

    return 0;
  }

  ssize_t PostRecv(struct iovec *iov, void **desc, size_t cnt, uint64_t tag, struct ofi_req *req) {
    struct ofi_context *ctx = nullptr;
    size_t offset = 0;

    while (offset < cnt) {
      auto num_iov = ((cnt - offset) > max_msg_iov_) ? max_msg_iov_ : (cnt - offset);
      OFI_CTX_NEW(ctx, req);
      OFI_POST(fi_trecvv(ep_.get(), iov + offset, desc, num_iov, 0, tag, 0, &ctx->ctx), PollRxCQ, ctx);
      offset += num_iov;
    }

    return 0;
  }

  ssize_t PostWrite(struct fi_msg_rma *msg, struct ofi_req *req) {
    struct ofi_context *ctx = nullptr;
    size_t offset = 0, iovcnt = msg->iov_count;
    auto msg_iov_head = msg->msg_iov;
    auto rma_iov_head = msg->rma_iov;
    auto desc_head = msg->desc;

    while (offset < iovcnt) {
      OFI_CTX_NEW(ctx, req);
      auto num_iov = ((iovcnt - offset) > max_rma_iov_) ? max_rma_iov_ : (iovcnt - offset);
      msg->msg_iov = msg_iov_head + offset;
      msg->iov_count = num_iov;
      msg->rma_iov = rma_iov_head + offset;
      msg->rma_iov_count = num_iov;
      msg->desc = desc_head + offset;
      msg->context = &ctx->ctx;
      OFI_POST(fi_writemsg(ep_.get(), msg, 0), PollTxCQ, ctx);
      offset += num_iov;
    }

    return 0;
  }

  ssize_t PostRead(struct fi_msg_rma *msg, struct ofi_req *req) {
    struct ofi_context *ctx = nullptr;
    size_t offset = 0, iovcnt = msg->iov_count;
    auto msg_iov_head = msg->msg_iov;
    auto rma_iov_head = msg->rma_iov;
    auto desc_head = msg->desc;

    while (offset < iovcnt) {
      OFI_CTX_NEW(ctx, req);
      auto num_iov = ((iovcnt - offset) > max_rma_iov_) ? max_rma_iov_ : (iovcnt - offset);
      msg->msg_iov = msg_iov_head + offset;
      msg->iov_count = num_iov;
      msg->rma_iov = rma_iov_head + offset;
      msg->rma_iov_count = num_iov;
      msg->desc = desc_head + offset;
      msg->context = &ctx->ctx;
      OFI_POST(fi_readmsg(ep_.get(), msg, 0), PollTxCQ, ctx);
      offset += num_iov;
    }

    return 0;
  }

  size_t PollTxCQ(size_t count, struct ofi_req *req) { return PollCQ(tx_cq_.get(), count, req); }

  size_t PollRxCQ(size_t count, struct ofi_req *req) { return PollCQ(rx_cq_.get(), count, req); }

  uint64_t GetNewSrcTag() const {
    static uint64_t source_tag = 1;
    return ((TAG_PROTO_MSG << OFI_TAG_SOURCE_BITS) | source_tag++);
  }

  void InsertAddr(const char *addr, uint16_t port, fi_addr_t *fi_addr) {
    int ret = fi_av_insertsvc(av_.get(), addr, std::to_string(port).c_str(), fi_addr, 0, nullptr);
    if (ret < 1) {
      OFI_PRINTERR(av_insert, ret);
    }
  }

  void InsertAddr(const void *addr, fi_addr_t *fi_addr) {
    int ret = fi_av_insert(av_.get(), addr, 1, fi_addr, 0, nullptr);
    if (ret < 1) {
      OFI_PRINTERR(av_insert, ret);
    }
  }

  void RemoveAddr(fi_addr_t *fi_addr) {
    int ret = fi_av_remove(av_.get(), fi_addr, 1, 0);
    if (ret < 0) {
      OFI_PRINTERR(av_remove, ret);
    }
  }

  struct ofi_addrinfo &GetBindAddrInfo() { return bind_addr_; }

 private:
  struct ofi_context {
    struct fi_context ctx;
    struct ofi_req *req;
  };

  template <typename T>
  using ofi_ptr = std::unique_ptr<T, std::function<void(T *)>>;

  template <typename T>
  static void fid_deleter(T *fd) { OFI_CALL(fi_close(reinterpret_cast<struct fid *>(fd)), fid_close); }

  ofi_ptr<struct fi_info> hints_;
  ofi_ptr<struct fi_info> info_;
  ofi_ptr<struct fid_fabric> fabric_;
  ofi_ptr<struct fid_domain> domain_;
  ofi_ptr<struct fid_cq> tx_cq_;
  ofi_ptr<struct fid_cq> rx_cq_;
  ofi_ptr<struct fid_av> av_;
  ofi_ptr<struct fid_ep> ep_;
  size_t max_msg_iov_;
  size_t max_rma_iov_;
  struct ofi_addrinfo bind_addr_;

  OFIEndpoint(const char *addr, const char *port)
      : hints_(fi_allocinfo(), fi_freeinfo), info_(nullptr, fi_freeinfo),
        fabric_(nullptr, fid_deleter<struct fid_fabric>), domain_(nullptr, fid_deleter<struct fid_domain>),
        tx_cq_(nullptr, fid_deleter<struct fid_cq>), rx_cq_(nullptr, fid_deleter<struct fid_cq>),
        av_(nullptr, fid_deleter<struct fid_av>), ep_(nullptr, fid_deleter<struct fid_ep>) {
    hints_->caps = FI_MSG | FI_RMA | FI_TAGGED;
    hints_->mode = FI_CONTEXT | FI_ASYNC_IOV;
    hints_->domain_attr->resource_mgmt = FI_RM_ENABLED;
    hints_->ep_attr->type = FI_EP_RDM;
    hints_->ep_attr->mem_tag_format = OFI_TAG_PROTO_MASK | OFI_TAG_SOURCE_MASK;
    hints_->rx_attr->msg_order = FI_ORDER_SAS;
    hints_->rx_attr->op_flags = FI_COMPLETION;
    hints_->rx_attr->total_buffered_recv = 0; // FI_RM_ENABLED ensures buffering of unexpected messages
    hints_->tx_attr->comp_order = FI_ORDER_NONE;
    hints_->tx_attr->msg_order = FI_ORDER_SAS;
    hints_->tx_attr->op_flags = FI_DELIVERY_COMPLETE | FI_COMPLETION;

    // get provider information
    struct fi_info *tmp_info = nullptr;
    OFI_CALL(fi_getinfo(OFI_VERSION, addr, port, 0, hints_.get(), &tmp_info), getinfo);
    info_.reset(tmp_info);

    max_msg_iov_ = std::min(info_->tx_attr->iov_limit, info_->rx_attr->iov_limit);
    max_rma_iov_ = info_->tx_attr->rma_iov_limit;

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
    cq_attr.format = FI_CQ_FORMAT_TAGGED;

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

    // get bind addr/port
    bind_addr_.addrlen = FI_NAME_MAX;
    OFI_CALL(fi_getname(&ep_->fid, &bind_addr_.addr, &bind_addr_.addrlen), getname);
  }

  size_t PollCQ(struct fid_cq *cq, size_t count, struct ofi_req *req) {
    ssize_t ret = 0;
    struct ofi_context *ctx = nullptr;
    struct fi_cq_err_entry cqe;

    while (req->comp < count) {
      // FIXME: implement timeout
      ret = fi_cq_read(cq, &cqe, 1);
      if (ret > 0) {
        ctx = container_of(cqe.op_context, struct ofi_context, ctx);
        OFI_CTX_COMP(ctx)++;
        OFI_CTX_FREE(ctx);
      } else if (ret < 0 && ret == -FI_EAVAIL) {
        ret = fi_cq_readerr(cq, &cqe, 0);
        if (ret < 0) {
          OFI_PRINTERR(cq_readerr, ret);
          break;
        }
        std::cerr << fi_cq_strerror(cq, cqe.prov_errno, cqe.err_data, nullptr, 0) << std::endl;
        ctx = container_of(cqe.op_context, struct ofi_context, ctx);
        OFI_CTX_COMP(ctx)++;
        OFI_CTX_FREE(ctx);
      } else if (ret < 0 && ret != -FI_EAGAIN) {
        OFI_PRINTERR(cq_read, ret);
        break;
      }
    }

    auto num_comp = req->comp;
    req->req -= num_comp;
    req->comp = 0;

    return num_comp;
  }
};

} // namespace ofi
} // namespace rnetlib

#endif // RNETLIB_OFI_OFI_ENDPOINT_H_
