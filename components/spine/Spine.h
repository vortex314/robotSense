/*
 * Spine.h
 *
 *  Created on: Nov 21, 2021
 *      Author: lieven
 */

#ifndef SRC_SPINE_H_
#define SRC_SPINE_H_

#include "CborReader.h"
#include "CborWriter.h"
#include "limero.h"

typedef enum {
  B_PUBLISH,   // RXD,TXD id , Bytes value
  B_SUBSCRIBE, // TXD id, string, qos,
  B_UNSUBSCRIBE,
  B_NODE,
  B_LOG
} MsgType;

#define FRAME_MAX 128

class Spine : public Actor {
  QueueFlow<Bytes> _framesRxd;

  CborReader _cborReader;
  CborWriter _cborWriter;

  std::string _loopbackTopic;
  std::string _latencyTopic;
  TimerSource _loopbackTimer;
  TimerSource _connectTimer;

public:
  ValueFlow<Bytes> rxdFrame;
  ValueFlow<Bytes> txdFrame;
  ValueFlow<CborReader> cborIn;
  ValueFlow<bool> connected;
  std::string dstPrefix;
  std::string srcPrefix;
  std::string node;

  Spine(Thread &thread);
  void init();
  void sendNode(const char *topic);
  void setNode(const char *);

  template <typename T> void publish(const char *topic, T v) {
    if (_cborWriter.reset()
            .array()
            .add(B_PUBLISH)
            .add(topic)
            .add(v)
            .close()
            .addCrc()
            .ok()) {
      txdFrame.on(_cborWriter.bytes());
    } else
      WARN(" CBOR serialization failed ");
  }

  template <typename T> Sink<T> &publisher(std::string topic) {
    std::string absTopic = srcPrefix + topic;
    if (topic.rfind("src/", 0) == 0 || topic.rfind("dst/", 0) == 0)
      absTopic = topic;
    SinkFunction<T> *sf = new SinkFunction<T>([&, absTopic](const T &t) {
      CborWriter cborWriter(100);
      // need a local copy as the thread can be different
      //      INFO("topic:%s", absTopic.c_str());
      if (cborWriter.reset()
              .array()
              .add(B_PUBLISH)
              .add(absTopic)
              .add(t)
              .close()
              .addCrc()
              .ok()) {
        txdFrame.on(_cborWriter.bytes());
      } else {
        WARN(" CBOR serialization failed ");
      }
    });
    return *sf;
  }

  template <typename T> Source<T> &subscriber(std::string topic) {
    std::string absTopic = dstPrefix + topic;
    if (topic.rfind("src/", 0) == 0 || topic.rfind("dst/", 0) == 0)
      absTopic = topic;
    auto lf = new LambdaFlow<CborReader, T>(
        [&, absTopic](T &t, const CborReader &cbor) {
          int type;
          std::string tpc;
          return (((CborReader &)cbor)
                      .parse()
                      .array()
                      .get(type)
                      .get(tpc)
                      .get(t)
                      .close()
                      .ok() &&
                  type == B_PUBLISH && tpc == absTopic);
        });
    cborIn >> lf;
    return *lf;
  }
};

#endif /* SRC_SPINE_H_ */
