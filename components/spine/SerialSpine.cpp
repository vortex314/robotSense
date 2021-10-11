
#include "SerialSpine.h"

#include <driver/uart.h>
//================================================================
class MsgFilter : public LambdaFlow<Bytes, Bytes> {
  int _msgType;
  CborDeserializer _fromCbor;

 public:
  MsgFilter(int msgType)
      : LambdaFlow<Bytes, Bytes>([&](Bytes &out, const Bytes &in) {
          int msgType;
          if (_fromCbor.fromBytes(in).begin().get(msgType).success() &&
              msgType == _msgType) {
            out = in;
            return true;
          }
          return false;
        }),
        _fromCbor(100) {
    _msgType = msgType;
  };
  static MsgFilter &nw(int msgType) { return *new MsgFilter(msgType); }
};

SerialSpine::SerialSpine(Thread &thr)
    : Actor(thr),
      _uart(UART::create(UART_NUM_0, 1, 3)),
      _toCbor(100),
      _fromCbor(100),
      _loopbackTimer(thr, 1000, true, "loopbackTimer"),
      _connectTimer(thr, 3000, true, "connectTimer"),
      _outgoing(10, "_outgoing"),
      _incoming(5, "_incoming") {
  node(Sys::hostname());
  _outgoing.async(thr);  // reduces stack need
  _incoming.async(thr);
  connected.async(thr);
}

SerialSpine::~SerialSpine() {}

void SerialSpine::node(const char *n) {
  _node = n;
  _srcPrefix = "src/" + _node + "/";
  _dstPrefix = "dst/" + _node + "/";
  _loopbackTopic = _dstPrefix + "system/loopback";
  connected = false;
};

int SerialSpine::init() {
  _uart.setClock(921600);
  _uart.onRxd(onRxd, this);
  _uart.mode("8N1");
  _uart.init();
  // outgoing
  _uptimePublisher = &publisher<uint64_t>("system/uptime");
  _latencyPublisher = &publisher<uint64_t>("system/latency");
  _loopbackSubscriber = &subscriber<uint64_t>("system/loopback");
  _loopbackPublisher = &publisher<uint64_t>(_loopbackTopic);

  /* _fromSerialFrame >> [&](const Bytes &bs)
  { LOGI << "RXD [" << bs.size() << "] " << cborDump(bs).c_str() << LEND; };
  _toSerialFrame >> [&](const Bytes &bs)
  { LOGI << "TXD [" << bs.size() << "] " << cborDump(bs).c_str() << LEND; };
*/
  _toSerialFrame >> _frameToBytes >>
      [&](const Bytes &bs) { _uart.write(bs.data(), bs.size()); };

  _outgoing >> [&](const PubMsg &msg) {
    if (_toCbor.begin()
            .add(B_PUBLISH)
            .add(msg.topic)
            .add(msg.payload)
            .end()
            .success())
      _toSerialFrame.on(_toCbor.toBytes());
  };

  // serialRxd >> [](const Bytes &bs)
  //{ INFO("RXD %d", bs.size()); };
  serialRxd >> _fromSerialFrame;

  _fromSerialFrame >> [&](const Bytes &msg) {
    int msgType;
    PubMsg pubMsg;
    if (_fromCbor.fromBytes(msg)
            .begin()
            .get(msgType)
            .get(pubMsg.topic)
            .get(pubMsg.payload)
            .end()
            .success() &&
        msgType == B_PUBLISH) {
      _incoming.on(pubMsg);
    } else
      WARN(" no publish Msg %s", cborDump(msg).c_str());
  };

  _loopbackTimer >> [&](const TimerMsg &tm) {
    if (!connected()) sendNode(Sys::hostname());
    _loopbackPublisher->on(Sys::micros());
  };

  subscriber<uint64_t>(_loopbackTopic) >> [&](const uint64_t &in) {
    _latencyPublisher->on(Sys::micros() - in);
    connected = true;
    INFO("connected");
    _connectTimer.reset();
  };

  _connectTimer >> [&](const TimerMsg &) {
    INFO("disconnected");
    connected = false;
  };

  return 0;
}

void SerialSpine::onRxd(void *me) {
  SerialSpine *spine = (SerialSpine *)me;
  while (spine->_uart.hasData()) {
    std::string str;
    spine->_uart.read(str);
    std::vector<uint8_t> bytes(str.begin(), str.end());
    bytes.push_back('\0');
    spine->serialRxd.emit(bytes);
  }
}

int SerialSpine::publish(std::string topic, Bytes &bs) {
  _outgoing.on({topic, bs});
  return 0;
}

int SerialSpine::subscribe(std::string pattern) {
  if (_toCbor.begin().add(B_SUBSCRIBE).add(pattern).end().success())
    _toSerialFrame.on(_toCbor.toBytes());
  return 0;
}

int SerialSpine::sendNode(std::string nodeName) {
  if (_toCbor.begin().add(B_NODE).add(nodeName).end().success())
    _toSerialFrame.on(_toCbor.toBytes());
  return 0;
}
