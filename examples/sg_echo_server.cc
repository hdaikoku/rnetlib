#include <iostream>

#include <rnetlib/rnetlib.h>

using namespace rnetlib;

int main(int argc, const char **argv) {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " [port] [num_sgs]" << std::endl;
    return 1;
  }

  int num_sgs = std::stoi(argv[2]);

  // FIXME: handle errors
  auto server = RNetLib::NewServer("0.0.0.0", static_cast<uint16_t>(std::stoul(argv[1])), RNetLib::Mode::SOCKET);
  server->Listen();
  auto channel = server->Accept();

  std::vector<std::unique_ptr<LocalMemoryRegion>> mrs;
  int msgs[num_sgs];
  for (int i = 0; i < num_sgs; i++) {
    msgs[i] = 0;
    mrs.emplace_back(channel->RegisterMemory(&msgs[i], sizeof(msgs[i]), MR_LOCAL_READ | MR_LOCAL_WRITE));
  }

  channel->RecvV(mrs);

  for (int i = 0; i < num_sgs; i++) {
    std::cout << "msgs[" << i << "]: " << msgs[i] << std::endl;
    msgs[i] *= 2;
  }

  channel->SendV(mrs);

  return 0;
}
