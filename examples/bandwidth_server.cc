//
// Created by Harunobu Daikoku on 2016/08/01.
//

#include <iostream>
#include <rnetlib/socket/socket_server.h>

using namespace rnetlib;
using namespace rnetlib::socket;

void do_pingpong(const Channel &channel, size_t msg_len) {
  std::unique_ptr<char[]> msg(new char[msg_len]);
  std::memset(msg.get(), 'a', msg_len);

  for (int i = 0; i < 500; i++) {
    if (channel.Recv(msg.get(), msg_len) != msg_len) {
      std::cerr << "ERROR: read" << std::endl;
      return;
    }

    if (channel.Send(msg.get(), msg_len) != msg_len) {
      std::cerr << "ERROR: write" << std::endl;
      return;
    }
  }
}

int main(int argc, const char **argv) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " [port]" << std::endl;
    return 1;
  }

  std::unique_ptr<Server> server(new SocketServer);
  // FIXME: handle errors
  server->Listen(std::stoi(argv[1]));
  auto channel = server->Accept();

  size_t max_bytes = (1 << 27);
  for (size_t i = 1; i <= max_bytes; i *= 2) {
    do_pingpong(*channel, i);
  }

  return 0;
}