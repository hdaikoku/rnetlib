#include <iostream>

#include <rnetlib/rnetlib.h>

int main(int argc, const char **argv) {
  if (argc != 4) {
    std::cerr << "Usage: " << argv[0] << " [addr] [port] [num_sgs]" << std::endl;
    return 1;
  }

  int num_sgs = std::stoi(argv[3]);

  // FIXME: handle errors
  auto client = rnetlib::NewClient(argv[1], static_cast<uint16_t>(std::stoul(argv[2])), rnetlib::PROV_SOCKET);
  auto channel = client->Connect();

  std::vector<rnetlib::LocalMemoryRegion::ptr> lmrs;
  int msgs[num_sgs];
  for (int i = 0; i < num_sgs; i++) {
    msgs[i] = i;
    lmrs.emplace_back(channel->RegisterMemoryRegion(&msgs[i], sizeof(msgs[i]),
                                                    rnetlib::MR_LOCAL_READ | rnetlib::MR_LOCAL_WRITE));
  }

  channel->SendV(lmrs.data(), lmrs.size());
  channel->RecvV(lmrs.data(), lmrs.size());

  for (int i = 0; i < num_sgs; i++) {
    std::cout << "msgs[" << i << "]: " << msgs[i] << std::endl;
  }

  return 0;
}
