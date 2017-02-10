//
// Created by Harunobu Daikoku on 2016/08/01.
//

#include <iostream>
#include <rnetlib/socket/socket_server.h>

using namespace rnetlib;
using namespace rnetlib::socket;

int main(int argc, const char **argv) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " [port]" << std::endl;
    return 1;
  }

  std::unique_ptr<Server> server(new SocketServer);
  // FIXME: handle errors
  server->Listen(std::stoi(argv[1]));
  auto channel = server->Accept();

  int msg;
  channel->Recv(&msg, sizeof(msg));
  std::cout << "SERVER: received " << msg << std::endl;

  channel->Send(&msg, sizeof(msg));

  return 0;
}