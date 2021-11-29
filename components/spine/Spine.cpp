/*
 * Spine.cpp
 *
 *  Created on: Nov 21, 2021
 *      Author: lieven
 */

#include "Spine.h"

Spine::Spine(Thread &thread)
    : Actor(thread), _framesRxd(5), _cborWriter(FRAME_MAX),
      _loopbackTimer(thread, 1000, true, "loopbackTimer"),
      _connectTimer(thread, 3000, true, "connectTimer") {
  setNode(Sys::hostname());
  rxdFrame >> _framesRxd;
  _framesRxd.async(thread);
  _framesRxd >> [&](const Bytes &bs) {
    int msgType;
    _cborReader.fill(bs);
    if (_cborReader.checkCrc()) {
      if (_cborReader.parse().array().get(msgType).ok()) {
        switch (msgType) {
        case B_PUBLISH: {
          cborIn.on(_cborReader);
          break;
        }
        case B_SUBSCRIBE:
        case B_NODE: {
          throw("I am not handling this");
          break;
        }
        }
      } else {
        INFO("get msgType failed");
      }
      _cborReader.close();
    } else {
      INFO("crc failed [%d] : %s", bs.size(), hexDump(bs).c_str());
    }
  };
}

void Spine::init() {
  _loopbackTimer >> [&](const TimerMsg &tm) {
    if (!connected())
      sendNode(node.c_str());
    publish(_loopbackTopic.c_str(), Sys::micros());
  };

  _connectTimer >> [&](const TimerMsg &) { connected = false; };

  subscriber<uint64_t>(_loopbackTopic) >> [&](const uint64_t &in) {
    publish(_latencyTopic.c_str(), (Sys::micros() - in) );
    connected = true;
    _connectTimer.reset();
  };
}

void Spine::setNode(const char *n) {
  node = n;
  srcPrefix = "src/" + node + "/";
  dstPrefix = "dst/" + node + "/";
  _loopbackTopic = dstPrefix + "system/loopback";
  _latencyTopic = srcPrefix + "system/latency";
  connected = false;
}

//================================= BROKER COMMANDS
//=========================================
void Spine::sendNode(const char *topic) {
  if (_cborWriter.reset().array().add(B_NODE).add(topic).close().addCrc().ok())
    txdFrame.on(_cborWriter.bytes());
  else
    WARN(" CBOR serialization failed ");
}
