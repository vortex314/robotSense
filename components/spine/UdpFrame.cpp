#include "UdpFrame.h"

UdpFrame *_UdpFrame = 0;
UdpFrame::UdpFrame(Thread &thread)
    : Actor(thread),
      rxdFrame(5),
      txdFrame([&](const Bytes &bs) { sendData(bs, "192.168.0.170");  }) {
  rxdFrame.async(thread);
  _rdPtr = 0;
  _wrPtr = 0;
  crcDMAdone = true;
  _UdpFrame = this;
}

bool UdpFrame::init() {
  online >> [&](const bool &on) {
    if (on)
      createSocket();
    else
      closeSocket();
  };
  return true;
}


void UdpFrame::sendData(const Bytes &bs, const char *dst) {
  if ( ! online()) return;
  struct sockaddr_in dstAddress;
  inet_pton(AF_INET,dst, &dstAddress.sin_addr);
  dstAddress.sin_port = htons(PORT);
  dstAddress.sin_family = AF_INET;

  int err = sendto(socket, bs.data(), bs.size(), 0,
                   (struct sockaddr *)&dstAddress, sizeof(dstAddress));
  if (err < 0 || err!=bs.size()) {
    ERROR("Error occurred during sending: errno %d :  %s", errno,strerror(errno));
    return;
  }
}

void UdpFrame::createSocket() {
  BZERO(dest_addr);
  dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  dest_addr.sin_family = AF_INET;
  dest_addr.sin_port = htons(PORT);
  ip_protocol = IPPROTO_IP;
  addr_family = AF_INET;
  socket = ::socket(addr_family, SOCK_DGRAM, ip_protocol);
  if (socket < 0) {
    ERROR("Unable to create socket: errno %d", errno);
    return;
  }
  INFO("Socket created");
  int err = bind(socket, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
  if (err < 0) {
    ERROR("Socket unable to bind: errno %d : %s", errno,strerror(errno));
    return;
  }
  INFO("Socket bound, port %d", PORT);
}

void UdpFrame::waitForData() {
  INFO("Waiting for data");
  struct sockaddr_storage source_addr;  // Large enough for both IPv4 or IPv6
  socklen_t socklen = sizeof(source_addr);
  int len = recvfrom(socket, rx_buffer, sizeof(rx_buffer) - 1, 0,
                     (struct sockaddr *)&source_addr, &socklen);

  if (len < 0) {
    ERROR("recvfrom failed: errno %d", errno);
    return;
  } else {
    if (source_addr.ss_family == PF_INET) {
      inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str,
                  sizeof(addr_str) - 1);
    } else if (source_addr.ss_family == PF_INET6) {
      inet6_ntoa_r(((struct sockaddr_in6 *)&source_addr)->sin6_addr, addr_str,
                   sizeof(addr_str) - 1);
    }

    rx_buffer[len] = 0;  // Null-terminate whatever we received and treat
                         // like a string...
    INFO("Received %d bytes from %s:", len, addr_str);
    INFO("%s", rx_buffer);

    int err = sendto(socket, rx_buffer, len, 0, (struct sockaddr *)&source_addr,
                     sizeof(source_addr));
    if (err < 0) {
      ERROR("Error occurred during sending: errno %d : %s ", errno,strerror(errno));
      return;
    }
  }
}

void UdpFrame::closeSocket() {
  if (socket != -1) {
    ERROR("Shutting down socket and restarting...");
    shutdown(socket, 0);
    close(socket);
  }
}
