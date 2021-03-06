#include <chrono>
#include <iostream>

#include <rnetlib/rnetlib.h>

void do_pingpong(rnetlib::Channel &channel, size_t msg_len) {
  std::unique_ptr<char[]> msg(new char[msg_len]);
  std::memset(msg.get(), 'a', msg_len);

  auto evloop = rnetlib::NewEventLoop(rnetlib::PROV_SOCKET);

  auto beg = std::chrono::steady_clock::now();
  for (int i = 0; i < 2500; i++) {
    channel.IRecv(msg.get(), msg_len, evloop);
    channel.ISend(msg.get(), msg_len, evloop);
  }
  evloop->WaitAll(300);
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
  auto server = rnetlib::NewServer("", static_cast<uint16_t>(std::stoul(argv[1])), rnetlib::PROV_SOCKET);
  server->Listen();
  auto channel = server->Accept();

  size_t max_bytes = (1 << 23);
  for (size_t i = 1; i <= max_bytes; i *= 2) {
    do_pingpong(*channel, i);
  }

  return 0;
}
