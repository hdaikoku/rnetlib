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
  auto lmr = channel->RegisterMemoryRegion(&msg, sizeof(msg), rnetlib::MR_REMOTE_WRITE | rnetlib::MR_LOCAL_READ);
  rnetlib::RemoteMemoryRegion rmr;

  channel->SynRemoteMemoryRegionV(&lmr, 1);
  channel->AckRemoteMemoryRegionV(&rmr, 1);

  int fin = 0;
  channel->Recv(&fin, sizeof(fin));
  assert(msg == 10 && fin == 1);
  std::cout << "EchoServer: received " << msg << std::endl;

  channel->Write(lmr, rmr);
  channel->Send(&fin, sizeof(fin));
  std::cout << "EchoServer: written " << msg << std::endl;

  return 0;
}
