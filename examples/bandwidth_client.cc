//
// Created by Harunobu Daikoku on 2016/08/01.
//

#include <iostream>
#include <rnetlib/socket/socket_client.h>

using namespace rnetlib;
using namespace rnetlib::socket;

void do_pingpong(const Channel &channel, size_t msg_len) {
  std::unique_ptr<char[]> msg(new char[msg_len]);
  std::memset(msg.get(), 'a', msg_len);

  auto beg = std::chrono::steady_clock::now();
  for (int i = 0; i < 500; i++) {
    if (channel.Send(msg.get(), msg_len) != msg_len) {
      std::cerr << "ERROR: write" << std::endl;
      return;
    }

    if (channel.Recv(msg.get(), msg_len) != msg_len) {
      std::cerr << "ERROR: read" << std::endl;
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

  std::unique_ptr<Client> client(new SocketClient);
  // FIXME: handle errors
  auto channel = client->Connect(argv[1], std::stoi(argv[2]));

  size_t max_bytes = (1 << 27);
  std::cout << "Length[Bytes]" << "\t" << "Bandwidth[Gbit/s]" << std::endl;
  for (size_t i = 1; i <= max_bytes; i *= 2) {
    do_pingpong(*channel, i);
  }

  return 0;
}