//
// Created by Harunobu Daikoku on 2016/08/01.
//

#include <chrono>
#include <iostream>
#include <rnetlib/rnetlib.h>

using namespace rnetlib;

void do_pingpong(Channel &channel, size_t msg_len) {
  std::unique_ptr<char[]> msg(new char[msg_len]);
  std::memset(msg.get(), 'a', msg_len);

  channel.SetNonBlocking(true);
  auto evloop = RNetLib::NewEventLoop(RNetLib::Mode::SOCKET);

  auto beg = std::chrono::steady_clock::now();
  for (int i = 0; i < 2500; i++) {
    channel.IRecv(msg.get(), msg_len, *evloop);
    channel.ISend(msg.get(), msg_len, *evloop);
  }
  evloop->Run(300);
  auto end = std::chrono::steady_clock::now();

  auto dur = std::chrono::duration_cast<std::chrono::microseconds>(end - beg).count();
  auto bw = (40. * msg_len) / dur;

  std::cout << msg_len << "\t" << bw << std::endl;
}

int main(int argc, const char **argv) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " [port]" << std::endl;
    return 1;
  }

  // FIXME: handle errors
  auto server = RNetLib::NewServer("0.0.0.0", static_cast<uint16_t>(std::stoul(argv[1])), RNetLib::Mode::SOCKET);
  server->Listen();
  auto channel = server->Accept();

  size_t max_bytes = (1 << 23);
  for (size_t i = 1; i <= max_bytes; i *= 2) {
    do_pingpong(*channel, i);
  }

  return 0;
}
