/*
 * UdpFrame.h
 *
 *  Created on: Nov 21, 2021
 *      Author: lieven
 */

#ifndef SRC_UART_H_
#define SRC_UART_H_

#include <Hardware.h>
#include <Log.h>
#include <lwip/netdb.h>
#include <string.h>
#include <sys/param.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "limero.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "nvs_flash.h"

#define PPP_MASK_CHAR 0x20
#define PPP_ESC_CHAR 0x7D
#define PPP_FLAG_CHAR 0x7E

#define FRAME_MAX 128

class UdpFrame : public Actor {
  Bytes _frameRxd;
  bool escFlag = false;
  size_t _wrPtr, _rdPtr;
  static void onRxd(void *);

 public:
  char addr_str[128];
  int addr_family;
  volatile bool crcDMAdone;
  uint32_t _txdOverflow = 0;
  uint32_t _rxdOverflow = 0;

  QueueFlow<Bytes> rxdFrame;
  SinkFunction<Bytes> txdFrame;
  ValueFlow<Bytes> txd;
  ValueFlow<bool> online;

#define PORT 9999

  char rx_buffer[128];
  int ip_protocol = 0;
  struct sockaddr_in6 dest_addr;
  int sock;

  UdpFrame(Thread &thread);
  bool init();
  void rxdByte(uint8_t);
  void sendBytes(uint8_t *, size_t);
  void sendFrame(const Bytes &bs);
  void createSocket();
  void closeSocket();
  void waitForData();
};

#endif /* SRC_UART_H_ */
