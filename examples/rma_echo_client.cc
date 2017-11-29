#include <iostream>

#include <rnetlib/rnetlib.h>

int main(int argc, const char **argv) {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " [addr] [port]" << std::endl;
    return 1;
  }

  // FIXME: handle errors
  auto client = rnetlib::NewClient(rnetlib::PROV_OFI);
  auto channel = client->Connect(argv[1], static_cast<uint16_t>(std::stoul(argv[2])));

  int msg = 10;
  auto lmr = channel->RegisterMemoryRegion(&msg, sizeof(msg), rnetlib::MR_REMOTE_WRITE | rnetlib::MR_LOCAL_READ);
  rnetlib::RemoteMemoryRegion rmr;

  channel->AckRemoteMemoryRegionV(&rmr, 1);
  channel->SynRemoteMemoryRegionV(&lmr, 1);

  int fin = 1;
  channel->Write(lmr, rmr);
  channel->Send(&fin, sizeof(fin));
  std::cout << "EchoClient: written " << msg << std::endl;

  channel->Recv(&fin, sizeof(fin));
  assert(msg == 10 && fin == 1);
  std::cout << "EchoClient: received " << msg << std::endl;

  return 0;
}
