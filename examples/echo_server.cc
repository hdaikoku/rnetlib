//
// Created by Harunobu Daikoku on 2016/08/01.
//

#include <iostream>
#include <rnetlib/rnetlib.h>

using namespace rnetlib;

int main(int argc, const char **argv) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " [port]" << std::endl;
    return 1;
  }

  // FIXME: handle errors
  auto server = RNetLib::NewServer("0.0.0.0", static_cast<uint16_t>(std::stoul(argv[1])), RNetLib::Mode::SOCKET);
  server->Listen();
  auto channel = server->Accept();

  int msg;
  channel->Recv(&msg, sizeof(msg));
  std::cout << "SERVER: received " << msg << std::endl;

  channel->Send(&msg, sizeof(msg));

  return 0;
}