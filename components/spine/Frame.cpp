#include <Frame.h>
#include <unistd.h>

//================================================================

BytesToFrame::BytesToFrame() : Flow<Bytes, String>() {}

void BytesToFrame::on(const Bytes &bs) { handleRxd(bs); }

void BytesToFrame::toStdout(const String &bs) {
  if (bs.size() > 1) {
    logs.on(bs);
    ::write(1, bs.data(), bs.size());
    //    LOGW << " extra ignored : " << hexDump(bs) << LEND;
  }
}

bool BytesToFrame::handleFrame(const Bytes &bs) {
  if (bs.size() == 0) return false;
  if (ppp_deframe(_cleanData, bs)) {
    String s((const char *)_cleanData.data(), _cleanData.size());
    emit(s);
    return true;
  } else {
    String s((const char *)bs.data(), bs.size());
    toStdout(s);
    return false;
  }
}

void BytesToFrame::handleRxd(const Bytes &bs) {
  for (uint8_t b : bs) {
    if (b == PPP_FLAG_CHAR) {
      _lastFrameFlag = Sys::millis();
      handleFrame(_inputFrame);
      _inputFrame.clear();
    } else {
      _inputFrame.push_back(b);
    }
  }
  if ((Sys::millis() - _lastFrameFlag) > _frameTimeout) {
    //   cout << " skip  Bytes " << hexDump(bs) << endl;
    //   cout << " frame data drop " << hexDump(frameData) << flush;
    String s((const char *)bs.data(), bs.size());
    toStdout(s);
    _inputFrame.clear();
  }
}
void BytesToFrame::request(){};

FrameToBytes::FrameToBytes()
    : LambdaFlow<String, Bytes>([&](Bytes &out, const String &in) {
        out = ppp_frame(
            Bytes((uint8_t *)in.c_str(), (uint8_t *)in.c_str() + in.length()));
        return true;
      }){};
