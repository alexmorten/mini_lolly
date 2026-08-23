// Serves the real firmware request handler over a localhost socket, so the
// simulator SPA can be driven against the actual device code.
#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include <string>

std::string NVS_ROOT;
SerialStub Serial;
WiFiStub WiFi;
MDNSStub MDNS;

#include "WebServer.cpp"

static bool readAll(int fd, std::string& header, std::string& body) {
  std::string buf;
  char tmp[2048];
  size_t headEnd = std::string::npos;
  while (headEnd == std::string::npos) {
    ssize_t n = recv(fd, tmp, sizeof(tmp), 0);
    if (n <= 0) return false;
    buf.append(tmp, n);
    headEnd = buf.find("\r\n\r\n");
  }
  header = buf.substr(0, headEnd + 4);
  body = buf.substr(headEnd + 4);
  size_t cl = header.find("Content-Length:");
  if (cl != std::string::npos) {
    size_t want = strtoul(header.c_str() + cl + 15, nullptr, 10);
    while (body.size() < want) {
      ssize_t n = recv(fd, tmp, sizeof(tmp), 0);
      if (n <= 0) break;
      body.append(tmp, n);
    }
  }
  return true;
}

int main(int argc, char** argv) {
  NVS_ROOT = argv[1];
  int port = atoi(argv[2]);
  PresetStore::begin();
  WifiNet::begin();

  int srv = socket(AF_INET, SOCK_STREAM, 0);
  int on = 1;
  setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
  sockaddr_in a{};
  a.sin_family = AF_INET;
  a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  a.sin_port = htons(port);
  if (bind(srv, (sockaddr*)&a, sizeof(a)) || listen(srv, 8)) {
    perror("bind/listen");
    return 1;
  }
  std::cout << "mini lolly host server on " << port << " (nvs " << NVS_ROOT << ")\n" << std::flush;

  for (;;) {
    int fd = accept(srv, nullptr, nullptr);
    if (fd < 0) continue;
    // On the device this runs every frame; here the only clock is a request, which
    // is enough because the Wi-Fi card polls while a join is in flight.
    WifiNet::loop();
    std::string header, body;
    if (readAll(fd, header, body)) {
      WiFiClient c;
      handleRequest(c, String(header), String(body));
      std::cout << header.substr(0, header.find("\r\n")) << " -> " << c.out.substr(0, c.out.find("\r\n"))
                << " (" << c.out.size() << " bytes)\n" << std::flush;
      send(fd, c.out.data(), c.out.size(), 0);
    }
    close(fd);
  }
}
