
#include "SerialSpineJson.h"

#include <driver/uart.h>

SerialSpine::SerialSpine(Thread &thr)
    : Actor(thr),
      _uart(UART::create(UART_NUM_0, 1, 3)),
      serialRxd(10, "RXD"),
      _loopbackTimer(thr, 1000, true, "loopbackTimer"),
      _connectTimer(thr, 3000, true, "connectTimer"),
      _incoming(5, "_incoming"),
      outgoing(10, "outgoing")

{
  setNode(Sys::hostname());
  outgoing.async(thr);  // reduces stack need
  _incoming.async(thr);
  connected.async(thr);
  serialRxd.async(thr);
}

SerialSpine::~SerialSpine() {}

void SerialSpine::setNode(const char *n) {
  node = n;
  srcPrefix = "src/" + node + "/";
  dstPrefix = "dst/" + node + "/";
  _loopbackTopic = dstPrefix + "system/loopback";
  connected = false;
};

int SerialSpine::init() {
  _uart.setClock(SERIAL_BAUD);
  _uart.onRxd(onRxd, this);
  _uart.mode("8N1");
  _uart.init();

  outgoing >> new SinkFunction<Json>([&](const Json &json) {
    if (connected()) _toSerialFrame.on(json.dump());
  });
  _toSerialFrame >> _frameToBytes >>
      [&](const Bytes &bs) { _uart.write(bs.data(), bs.size()); };

  serialRxd >> _fromSerialFrame >> [&](const String &frame) {
    INFO("%s", frame.c_str());
    _incoming.on(Json::parse(frame));
  };

  Source<uint64_t> &_loopbackSubscriber = subscriber<uint64_t>(_loopbackTopic);
  Sink<uint64_t> &_latencyPublisher = publisher<uint64_t>("system/latency");

  _loopbackTimer >> [&](const TimerMsg &tm) {
    if (!connected()) sendNode(Sys::hostname());
    _toSerialFrame.on(Json({B_PUBLISH, _loopbackTopic, Sys::micros()}).dump());
    // short circuit outgoing filter if not connected
  };

  _loopbackSubscriber >> [&](const uint64_t &in) {
    _latencyPublisher.on(Sys::micros() - in);
    connected = true;
    _connectTimer.reset();
    INFO("connected");
  };

  _connectTimer >> [&](const TimerMsg &) {
    connected = false;
    INFO("disconnected");
  };

  return 0;
}

void SerialSpine::onRxd(void *me) {
  SerialSpine *spine = (SerialSpine *)me;
  Bytes bytes;
  while (spine->_uart.hasData()) {
    bytes.push_back(spine->_uart.read());
  }
  spine->serialRxd.emit(bytes);
}

int SerialSpine::publish(String topic, String &bs) {
  outgoing.on({topic, bs});
  return 0;
}

int SerialSpine::subscribe(String pattern) {
  outgoing.on({B_SUBSCRIBE, pattern});
  return 0;
}

int SerialSpine::sendNode(String nodeName) {
  _toSerialFrame.on(Json({B_NODE, nodeName}).dump());
  // short circuit outgoing filter if not
  // connected({B_NODE, nodeName});
  return 0;
}
