#include <chrono>
#include <iostream>
#include <thread>

#include <rnetlib/rnetlib.h>

ssize_t send_by_iovec(const std::vector<rnetlib::LocalMemoryRegion::ptr> &mrs, rnetlib::Channel::ptr &channel) {
  auto beg = std::chrono::steady_clock::now();
  channel->RecvV(mrs.data(), mrs.size());
  auto end = std::chrono::steady_clock::now();
  return std::chrono::duration_cast<std::chrono::milliseconds>(end - beg).count();
}

ssize_t send_by_iter(const std::vector<rnetlib::LocalMemoryRegion::ptr> &lmrs, rnetlib::Channel::ptr &channel) {
  auto beg = std::chrono::steady_clock::now();
  for (const auto &lmr : lmrs) {
    channel->Recv(lmr);
  }
  auto end = std::chrono::steady_clock::now();

  return std::chrono::duration_cast<std::chrono::milliseconds>(end - beg).count();
}

ssize_t send_by_rma(const std::vector<rnetlib::LocalMemoryRegion::ptr> &lmrs, rnetlib::Channel::ptr &channel) {
  auto beg = std::chrono::steady_clock::now();
  channel->SynRemoteMemoryRegionV(lmrs.data(), lmrs.size());

  int fin = 0;
  channel->Recv(&fin, sizeof(fin));
  auto end = std::chrono::steady_clock::now();

  return std::chrono::duration_cast<std::chrono::milliseconds>(end - beg).count();
}

void print_bw(const std::string &desc, size_t msg_size, int num_iters, ssize_t dur_msecs) {
  std::cout << "[SGBandwidthServer] " << desc << ": " << dur_msecs << " [msecs], Bandwidth: "
            << (msg_size * num_iters * 8.) / (dur_msecs * 1000000.) << " [Gbit/s]" << std::endl;
}

int main(int argc, const char **argv) {
  if (argc != 4) {
    std::cout << "Usage: " << argv[0] << " [port] [msg_size] [num_iters]" << std::endl;
    return 1;
  }

  size_t msg_size = std::stoul(argv[2]);
  int num_iters = std::stoi(argv[3]);

  // FIXME: handle errors
  auto server = rnetlib::NewServer("", static_cast<uint16_t>(std::stoul(argv[1])), rnetlib::PROV_SOCKET);
  server->Listen();
  auto channel = server->Accept();

  std::vector<std::unique_ptr<char[]>> blks;
  std::vector<rnetlib::LocalMemoryRegion::ptr> lmrs;
  for (int i = 0; i < num_iters; i++) {
    std::unique_ptr<char[]> blk(new char[msg_size]);
    lmrs.emplace_back(
        channel->RegisterMemoryRegion(blk.get(), msg_size,
                                      rnetlib::MR_LOCAL_READ | rnetlib::MR_LOCAL_WRITE | rnetlib::MR_REMOTE_READ)
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
