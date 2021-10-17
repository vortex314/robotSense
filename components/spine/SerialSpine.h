#ifndef BrokerSerial_H
#define BrokerSerial_H
#include <CborDeserializer.h>
#include <CborDump.h>
#include <CborSerializer.h>
#include <Frame.h>
#include <Hardware.h>
#include <Log.h>
#include <broker_protocol.h>
#include <limero.h>

struct PubMsg {
  std::string topic;
  Bytes payload;
};

class SerialSpine : Actor {
  UART &_uart;
  ValueFlow<Bytes> serialRxd;
  Sink<uint64_t> *_uptimePublisher;
  Sink<uint64_t> *_latencyPublisher;
  Sink<uint64_t> *_loopbackPublisher;
  Source<uint64_t> *_loopbackSubscriber;
  ValueFlow<Bytes> _toSerialFrame;

  BytesToFrame _fromSerialFrame;
  FrameToBytes _frameToBytes;
  CborSerializer _toCbor;
  CborDeserializer _fromCbor;

  std::string _loopbackTopic;
  TimerSource _loopbackTimer;
  TimerSource _connectTimer;
  QueueFlow<PubMsg> _incoming;

 public:
  std::string node;
  std::string dstPrefix;
  std::string srcPrefix;
  QueueFlow<PubMsg> outgoing;
  ValueFlow<bool> connected;

  static void onRxd(void *);
  SerialSpine(Thread &thr);
  ~SerialSpine();
  int init();
  int publish(std::string, Bytes &);
  int subscribe(std::string);
  int sendNode(std::string);
  void setNode(const char *);

  template <typename T>
  Sink<T> &publisher(std::string topic) {
    std::string absTopic = srcPrefix + topic;
    if (topic.rfind("src/", 0) == 0 || topic.rfind("dst/", 0) == 0)
      absTopic = topic;
    SinkFunction<T> *sf = new SinkFunction<T>([&, absTopic](const T &t) {
      //      INFO("topic:%s", absTopic.c_str());
      if (_toCbor.begin().add(t).end().success()) {
        outgoing.on({absTopic, _toCbor.toBytes()});
      } else {
        WARN(" CBOR serialization failed ");
      }
    });
    return *sf;
  }
  template <typename T>
  Source<T> &subscriber(std::string topic) {
    std::string absTopic = dstPrefix + topic;
    if (topic.rfind("src/", 0) == 0 || topic.rfind("dst/", 0) == 0)
      absTopic = topic;
    auto lf = new LambdaFlow<PubMsg, T>([&, absTopic](T &t, const PubMsg &msg) {
      if (absTopic == msg.topic)
        return _fromCbor.fromBytes(msg.payload).begin().get(t).success();
      else
        return false;
    });
    _incoming >> lf;
    return *lf;
  }
};

#endif  // BrokerSerial_H
