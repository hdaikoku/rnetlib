#include <iostream>

#include <rnetlib/rnetlib.h>

int main(int argc, const char **argv) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " [port]" << std::endl;
    return 1;
  }

  // FIXME: handle errors
  auto server = rnetlib::NewServer("", static_cast<uint16_t>(std::stoul(argv[1])), rnetlib::PROV_SOCKET);
  server->Listen();
  std::cout << "EchoServer: listening on port " << server->GetListenPort() << std::endl;
  auto channel = server->Accept();

  int msg = 0;
  channel->Recv(&msg, sizeof(msg));
  std::cout << "EchoServer: received " << msg << std::endl;

  channel->Send(&msg, sizeof(msg));
  std::cout << "EchoServer: sent " << msg << std::endl;

  return 0;
}
