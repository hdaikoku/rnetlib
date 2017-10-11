#include <iostream>
#include <thread>

#include <rnetlib/rnetlib.h>

using namespace rnetlib;

void send_by_iovec(const std::vector<std::unique_ptr<LocalMemoryRegion>> &mrs,
                   size_t msg_size,
                   int num_iters,
                   Channel &channel) {
  channel.SetNonBlocking(true);
  auto evloop = RNetLib::NewEventLoop(RNetLib::Mode::SOCKET);

  auto beg = std::chrono::steady_clock::now();
  channel.IRecvVec(mrs, *evloop);
  channel.ISendVec(mrs, *evloop);
  evloop->WaitAll(300000);
  auto end = std::chrono::steady_clock::now();
  auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(end - beg).count();
  auto bw = (msg_size * num_iters * 16.) / (dur * 1000000.);

  std::cout << "send by iovec: " << dur << " [msecs], Bandwidth: " << bw << " [Gbit/s]" << std::endl;
}

void send_by_iter(const std::vector<std::unique_ptr<char[]>> &blks, size_t msg_size, int num_iters, Channel &channel) {
  auto beg = std::chrono::steady_clock::now();
  for (int i = 0; i < num_iters; i++) {
    channel.Send(blks[i].get(), msg_size);
    channel.Recv(blks[i].get(), msg_size);
  }
  auto end = std::chrono::steady_clock::now();
  auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(end - beg).count();

  std::cout << "send by iter.: " << dur << " [msecs], Bandwidth: "
            << (msg_size * num_iters * 16) / (dur * 1000000) << " [Gbit/s]" << std::endl;
}

int main(int argc, const char **argv) {
  if (argc != 5) {
    std::cerr << "Usage: " << argv[0] << " [addr] [port] [msg_size] [num_iters]" << std::endl;
    return 1;
  }

  size_t msg_size = std::stoi(argv[3]);
  int num_iters = std::stoi(argv[4]);

  // FIXME: handle errors
  auto client = RNetLib::NewClient(argv[1], static_cast<uint16_t>(std::stoul(argv[2])), RNetLib::Mode::SOCKET);
  auto channel = client->Connect();

  std::vector<std::unique_ptr<char[]>> blks;
  std::vector<std::unique_ptr<LocalMemoryRegion>> mrs;
  for (int i = 0; i < num_iters; i++) {
    std::unique_ptr<char[]> blk(new char[msg_size]);
    mrs.emplace_back(channel->RegisterMemory(blk.get(), msg_size, MR_LOCAL_READ | MR_LOCAL_WRITE));
    blks.emplace_back(std::move(blk));
  }

  for (int i = 0; i < 4; i++) {
    send_by_iovec(mrs, msg_size, num_iters, *channel);
    //send_by_iter(blks, msg_size, num_iters, *channel);
    std::this_thread::sleep_for(std::chrono::seconds(3));
  }

  return 0;
}
