#include <iostream>

#include <rnetlib/rnetlib.h>

void do_pingpong(const rnetlib::Channel &channel, size_t msg_len) {
  std::unique_ptr<char[]> msg(new char[msg_len]);
  std::memset(msg.get(), 'a', msg_len);

  auto local_mem = channel.RegisterMemory(msg.get(), msg_len, MR_LOCAL_WRITE);

  for (int i = 0; i < 5000; i++) {
    if (channel.Recv(*local_mem) != msg_len) {
      std::cerr << "ERROR: recv" << std::endl;
      return;
    }
  }
}

int main(int argc, const char **argv) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " [port]" << std::endl;
    return 1;
  }

  // FIXME: handle errors
  auto server = rnetlib::NewServer("0.0.0.0", static_cast<uint16_t>(std::stoul(argv[1])), rnetlib::Mode::SOCKET);
  server->Listen();
  auto channel = server->Accept();

  size_t max_bytes = (1 << 23);
  for (size_t i = 1; i <= max_bytes; i *= 2) {
    do_pingpong(*channel, i);
  }

  return 0;
}
