// Fuzzer that runs darkhttpd in a background thread.
#include <arpa/inet.h>
#include <err.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

#include <thread>

extern "C" int darkhttpd(int argc, const char** argv);

namespace {
int argc = 4;
const char* argv[] = {"./a.out", "tmp.fuzz", "--log", "/dev/null"};
std::thread* thr;
const char* host = "127.0.0.1";
int port = 8080;
struct sockaddr_in addrin;
}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static bool inited = false;
  if (!inited) {
    thr = new std::thread([]() { darkhttpd(argc, argv); });
    addrin.sin_family = AF_INET;
    addrin.sin_port = htons(port);
    if (inet_aton(host, &addrin.sin_addr) == 0) err(1, "inet_aton");
    inited = true;
    sleep(1);
  }

  char buf[4096];
  ssize_t rcvd, sent;

  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd == -1) err(1, "socket");

  if (connect(fd, (const struct sockaddr*)&addrin, sizeof(struct sockaddr)) ==
      -1)
    err(1, "connect");

  sent = send(fd, data, size, 0);
  if (sent != size) err(1, "send");

  send(fd, "\n\n\n", 3, 0);  // To finish the request so we can recv().

  rcvd = recv(fd, buf, sizeof(buf), 0);
  // if (rcvd == -1) err(1, "recv");
  close(fd);

  return 0;
}

/* vim:set ts=2 sw=2 sts=2 expandtab tw=78: */
