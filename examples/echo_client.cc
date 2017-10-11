#include <iostream>

#include <rnetlib/rnetlib.h>

using namespace rnetlib;

int main(int argc, const char **argv) {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " [addr] [port]" << std::endl;
    return 1;
  }

  // FIXME: handle errors
  auto client = RNetLib::NewClient(argv[1], static_cast<uint16_t>(std::stoul(argv[2])), RNetLib::Mode::SOCKET);
  auto channel = client->Connect();

  int msg = 10;
  channel->Send(&msg, sizeof(msg));

  channel->Recv(&msg, sizeof(msg));
  std::cout << "EchoClient: received " << msg << std::endl;

  return 0;
}
