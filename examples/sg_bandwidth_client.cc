#include <iostream>
#include <thread>

#include <rnetlib/rnetlib.h>

ssize_t send_by_iovec(const std::vector<rnetlib::LocalMemoryRegion::ptr> &mrs, rnetlib::Channel::ptr &channel) {
  auto beg = std::chrono::steady_clock::now();
  channel->SendV(mrs.data(), mrs.size());
  auto end = std::chrono::steady_clock::now();
  return std::chrono::duration_cast<std::chrono::milliseconds>(end - beg).count();
}

ssize_t send_by_iter(const std::vector<rnetlib::LocalMemoryRegion::ptr> &lmrs, rnetlib::Channel::ptr &channel) {
  auto beg = std::chrono::steady_clock::now();
  for (const auto &lmr : lmrs) {
    channel->Send(lmr);
  }
  auto end = std::chrono::steady_clock::now();

  return std::chrono::duration_cast<std::chrono::milliseconds>(end - beg).count();
}

ssize_t send_by_rma(const std::vector<rnetlib::LocalMemoryRegion::ptr> &lmrs, rnetlib::Channel::ptr &channel) {
  auto beg = std::chrono::steady_clock::now();
  std::vector<rnetlib::RemoteMemoryRegion> rmrs(lmrs.size());
  channel->AckRemoteMemoryRegionV(rmrs.data(), rmrs.size());

  channel->ReadV(lmrs.data(), rmrs.data(), lmrs.size());
  int fin = 1;
  channel->Send(&fin, sizeof(fin));
  auto end = std::chrono::steady_clock::now();

  return std::chrono::duration_cast<std::chrono::milliseconds>(end - beg).count();
}

void print_bw(const std::string &desc, size_t msg_size, int num_iters, ssize_t dur_msecs) {
  std::cout << "[SGBandwidthClient] " << desc << ": " << dur_msecs << " [msecs], Bandwidth: "
            << (msg_size * num_iters * 8.) / (dur_msecs * 1000000.) << " [Gbit/s]" << std::endl;
}

int main(int argc, const char **argv) {
  if (argc != 5) {
    std::cerr << "Usage: " << argv[0] << " [addr] [port] [msg_size] [num_iters]" << std::endl;
    return 1;
  }

  size_t msg_size = std::stoul(argv[3]);
  int num_iters = std::stoi(argv[4]);

  // FIXME: handle errors
  auto client = rnetlib::NewClient(rnetlib::PROV_OFI);
  auto channel = client->Connect(argv[1], static_cast<uint16_t>(std::stoul(argv[2])));

  std::vector<std::unique_ptr<char[]>> blks;
  std::vector<rnetlib::LocalMemoryRegion::ptr> lmrs;
  for (int i = 0; i < num_iters; i++) {
    std::unique_ptr<char[]> blk(new char[msg_size]);
    lmrs.emplace_back(
        channel->RegisterMemoryRegion(blk.get(), msg_size, rnetlib::MR_LOCAL_READ | rnetlib::MR_LOCAL_WRITE)
    );
    blks.emplace_back(std::move(blk));
  }

  ssize_t dur = 0;
  for (int i = 0; i < 3; i++) {
    dur = send_by_iovec(lmrs, channel);
    print_bw("send_by_iovec", msg_size, num_iters, dur);
    std::this_thread::sleep_for(std::chrono::seconds(2));
    dur = send_by_iter(lmrs, channel);
    print_bw("send_by_iter", msg_size, num_iters, dur);
    std::this_thread::sleep_for(std::chrono::seconds(2));
    dur = send_by_rma(lmrs, channel);
    print_bw("send_by_rma", msg_size, num_iters, dur);
    std::this_thread::sleep_for(std::chrono::seconds(2));
  }

  return 0;
}
