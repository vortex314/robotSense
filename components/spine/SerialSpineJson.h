#ifndef BrokerSerial_H
#define BrokerSerial_H
#include <nlohmann/json.hpp>
using Json = nlohmann::json;

#include <Frame.h>
#include <Hardware.h>
#include <Log.h>
#include <broker_protocol.h>
#include <limero.h>

struct PubMsg {
  String topic;
  String payload;
};

class SerialSpine : Actor {
  UART &_uart;
  QueueFlow<Bytes> serialRxd;

  BytesToFrame _fromSerialFrame;
  FrameToBytes _frameToBytes;

  String _loopbackTopic;
  TimerSource _loopbackTimer;
  TimerSource _connectTimer;
  QueueFlow<Json> _incoming;

 public:
  String node;
  String dstPrefix;
  String srcPrefix;
  QueueFlow<Json> outgoing;
  ValueFlow<String> _toSerialFrame;
  ValueFlow<bool> connected;

  static void onRxd(void *);
  SerialSpine(Thread &thr);
  ~SerialSpine();
  int init();
  int publish(String, String &);
  int publish(String, Json &);

  int subscribe(String);
  int sendNode(String);
  void setNode(const char *);

  template <typename T>
  Sink<T> &publisher(String topic) {
    String absTopic = srcPrefix + topic;
    if (topic.rfind("src/", 0) == 0 || topic.rfind("dst/", 0) == 0)
      absTopic = topic;
    SinkFunction<T> *sf = new SinkFunction<T>([&, absTopic](const T &t) {
      outgoing.on({B_PUBLISH, absTopic, t});
    });
    return *sf;
  }

  template <typename T>
  Source<T> &subscriber(String pattern) {
    String absPattern = dstPrefix + pattern;
    if (pattern.rfind("src/", 0) == 0 || pattern.rfind("dst/", 0) == 0)
      absPattern = pattern;
    auto lf = new LambdaFlow<Json, T>([&, absPattern](T &t, const Json &json) {
      if (json[0] == B_PUBLISH &&
          json[1] == absPattern /*|| match(absPattern, msg.topic)*/) {
        t = json[2].get<T>();
        return true;
      }
      return false;
    });
    _incoming >> lf;
    return *lf;
  }
};

#endif  // BrokerSerial_H
