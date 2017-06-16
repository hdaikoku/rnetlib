//
// Created by Harunobu Daikoku on 2017/06/15.
//

#include <iostream>
#include <rnetlib/rnetlib.h>

using namespace rnetlib;

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

  auto beg = std::chrono::steady_clock::now();
  for (int i = 0; i < num_iters; i++) {
    channel->Send(blks[i].get(), msg_size);
    channel->Recv(blks[i].get(), msg_size);
  }
  auto end = std::chrono::steady_clock::now();
  auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(end - beg).count();

  std::cout << "send by iter.: " << dur << " [msecs], Bandwidth: "
            << (msg_size * num_iters * 16) / (dur * 1000000) << " [Gbit/s]" << std::endl;

  beg = std::chrono::steady_clock::now();
  channel->SendSG(mrs);
  channel->RecvSG(mrs);
  end = std::chrono::steady_clock::now();
  dur = std::chrono::duration_cast<std::chrono::milliseconds>(end - beg).count();

  std::cout << "send by iovec: " << dur << " [msecs], Bandwidth: "
            << (msg_size * num_iters * 16) / (dur * 1000000) << " [Gbit/s]" << std::endl;

  return 0;
}