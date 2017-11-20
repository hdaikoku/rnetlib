#include <iostream>

#include <rnetlib/rnetlib.h>

int main(int argc, const char **argv) {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " [port] [num_sgs]" << std::endl;
    return 1;
  }

  int num_sgs = std::stoi(argv[2]);

  // FIXME: handle errors
  auto server = rnetlib::NewServer("", static_cast<uint16_t>(std::stoul(argv[1])), rnetlib::Mode::SOCKET);
  server->Listen();
  auto channel = server->Accept();

  std::vector<rnetlib::LocalMemoryRegion::ptr> lmrs;
  int msgs[num_sgs];
  for (int i = 0; i < num_sgs; i++) {
    msgs[i] = 0;
    lmrs.emplace_back(channel->RegisterMemoryRegion(&msgs[i], sizeof(msgs[i]),
                                                    rnetlib::MR_LOCAL_READ | rnetlib::MR_LOCAL_WRITE));
  }

  channel->RecvV(lmrs.data(), lmrs.size());

  for (int i = 0; i < num_sgs; i++) {
    std::cout << "msgs[" << i << "]: " << msgs[i] << std::endl;
    msgs[i] *= 2;
  }

  channel->SendV(lmrs.data(), lmrs.size());

  return 0;
}
