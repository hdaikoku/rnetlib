//
// Created by Harunobu Daikoku on 2016/08/01.
//

#include <chrono>
#include <iostream>
#include <rnetlib/rnetlib.h>

using namespace rnetlib;

void do_pingpong(const Channel &channel, size_t msg_len) {
  std::unique_ptr<char[]> msg(new char[msg_len]);
  std::memset(msg.get(), 'a', msg_len);

  auto local_mem = channel.RegisterMemory(msg.get(), msg_len, MR_LOCAL_WRITE);
  auto remote_mem = channel.AckRemoteMemoryRegion();

  auto beg = std::chrono::steady_clock::now();
  for (int i = 0; i < 500; i++) {
    if (channel.Write(*local_mem, *remote_mem) != msg_len) {
      std::cerr << "ERROR: write" << std::endl;
      return;
    }
  }
  auto end = std::chrono::steady_clock::now();

  auto dur = std::chrono::duration_cast<std::chrono::microseconds>(end - beg).count();
  auto bw = (8.0 * msg_len) / dur;

  std::cout << msg_len << "\t" << bw << std::endl;
}

int main(int argc, const char **argv) {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " [addr] [port]" << std::endl;
    return 1;
  }

  RNetLib::SetMode(RNetLib::Mode::SOCKET);

  // FIXME: handle errors
  auto client = RNetLib::NewClient(argv[1], static_cast<uint16_t>(std::stoul(argv[2])));
  auto channel = client->Connect();

  size_t max_bytes = (1 << 27);
  std::cout << "Length[Bytes]" << "\t" << "Bandwidth[Gbit/s]" << std::endl;
  for (size_t i = 1; i <= max_bytes; i *= 2) {
    do_pingpong(*channel, i);
  }

  return 0;
}