#include <iostream>

#include <rnetlib/rnetlib.h>

int main(int argc, const char **argv) {
  if (argc != 4) {
    std::cerr << "Usage: " << argv[0] << " [addr] [port] [num_sgs]" << std::endl;
    return 1;
  }

  int num_sgs = std::stoi(argv[3]);

  // FIXME: handle errors
  auto client = rnetlib::NewClient(argv[1], static_cast<uint16_t>(std::stoul(argv[2])), rnetlib::Mode::SOCKET);
  auto channel = client->Connect();

  std::vector<rnetlib::LocalMemoryRegion::ptr> mrs;
  int msgs[num_sgs];
  for (int i = 0; i < num_sgs; i++) {
    msgs[i] = i;
    mrs.emplace_back(channel->RegisterMemoryRegion(&msgs[i], sizeof(msgs[i]), MR_LOCAL_READ | MR_LOCAL_WRITE));
  }
  channel->SendV(mrs.data(), mrs.size());

  channel->RecvV(mrs.data(), mrs.size());

  for (int i = 0; i < num_sgs; i++) {
    std::cout << "msgs[" << i << "]: " << msgs[i] << std::endl;
  }

  return 0;
}
