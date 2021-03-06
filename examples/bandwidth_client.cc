#include <chrono>
#include <iostream>

#include <rnetlib/rnetlib.h>

void do_pingpong(rnetlib::Channel &channel, size_t msg_len) {
  std::unique_ptr<char[]> msg(new char[msg_len]);
  std::memset(msg.get(), 'a', msg_len);

  auto lmr = channel.RegisterMemoryRegion(msg.get(), msg_len, rnetlib::MR_LOCAL_WRITE);

  auto beg = std::chrono::steady_clock::now();
  for (int i = 0; i < 5000; i++) {
    if (channel.Send(lmr) != msg_len) {
      std::cerr << "ERROR: send" << std::endl;
      return;
    }
  }
  auto end = std::chrono::steady_clock::now();

  auto dur = std::chrono::duration_cast<std::chrono::microseconds>(end - beg).count();
  auto bw = (40. * msg_len) / dur;

  std::cout << msg_len << "\t" << bw << std::endl;
}

int main(int argc, const char **argv) {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " [addr] [port]" << std::endl;
    return 1;
  }

  // FIXME: handle errors
  auto client = rnetlib::NewClient(rnetlib::PROV_SOCKET);
  auto channel = client->Connect(argv[1], static_cast<uint16_t>(std::stoul(argv[2])));

  size_t max_bytes = (1 << 23);
  std::cout << "Length[Bytes]" << "\t" << "Bandwidth[Gbit/s]" << std::endl;
  for (size_t i = 1; i <= max_bytes; i *= 2) {
    do_pingpong(*channel, i);
  }

  return 0;
}
