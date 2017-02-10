//
// Created by Harunobu Daikoku on 2016/08/01.
//

#include <iostream>
#include <rnetlib/socket/socket_client.h>

using namespace rnetlib;
using namespace rnetlib::socket;

int main(int argc, const char **argv) {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " [addr] [port]" << std::endl;
    return 1;
  }

  std::unique_ptr<Client> client(new SocketClient);
  // FIXME: handle errors
  auto channel = client->Connect(argv[1], std::stoi(argv[2]));

  int msg = 10;
  channel->Send(&msg, sizeof(msg));

  channel->Recv(&msg, sizeof(msg));
  std::cout << "CLIENT: received " << msg << std::endl;

  return 0;
}
