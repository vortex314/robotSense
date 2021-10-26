/*
 * Log.cpp
 *
 *  Created on: Jul 3, 2016
 *      Author: lieven
 */
#include <Log.h>
extern "C" {
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
}

char Log::_logLevel[7] = {'T', 'D', 'I', 'W', 'E', 'F', 'N'};


void bytesToHex(std::string &ret, uint8_t *input, uint32_t length, char sep) {
  static const char characters[] = "0123456789ABCDEF";
  for (uint32_t i = 0; i < length; i++) {
    ret += (characters[input[i] >> 4]);
    ret += characters[input[i] & 0x0F];
    if (sep != 0) ret += sep;
  }
}

void Log::serialLog(char *start, uint32_t length) {
  *(start + length) = '\0';
  ::printf("%s\n", start);
}

Log::Log(uint32_t size)
    : _enabled(true),
      _logFunction(serialLog),
      _level(LOG_INFO),
      _sema(Sema::create()) {
  if (_line == 0) {
    _line = new std::string;
    _line->reserve(size);
  }
  _application[0] = 0;
  _hostname[0] = 0;
}

Log::~Log() {}

void Log::setLogLevel(char c) {
  for (uint32_t i = 0; i < sizeof(_logLevel); i++)
    if (_logLevel[i] == c) {
      _level = (Log::LogLevel)i;
      break;
    }
}

bool Log::enabled(LogLevel level) {
  if (level >= _level) {
    return true;
  }
  return false;
}

void Log::disable() { _enabled = false; }

void Log::enable() { _enabled = true; }

void Log::defaultOutput() { _logFunction = serialLog; }

void Log::writer(LogFunction function) { _logFunction = function; }

LogFunction Log::writer() { return _logFunction; }

void Log::application(const char *app) {
  strncpy(_application, app, sizeof(_application));
}

void Log::log(char level, const char *file, uint32_t lineNbr,
              const char *function, const char *fmt, ...) {
  _sema.wait();
  if (_line == 0) {
    ::printf("%s:%d %s:%u\n", __FILE__, __LINE__, file, (unsigned int)lineNbr);
    //		_sema.release();
    return;
  }

  va_list args;
  va_start(args, fmt);
  static char logLine[256];
  vsnprintf(logLine, sizeof(logLine) - 1, fmt, args);
  va_end(args);

  extern void *pxCurrentTCB;
  //	if ( _application[0]==0)
  ::snprintf(_application, sizeof(_application), "%X", (uint32_t)pxCurrentTCB);
  *_line = stringFormat("%+10.10s %c | %8s | %s | %15s:%4d | %s", _application,
                level, time(), Sys::hostname(), file, lineNbr, logLine);
  logger.flush();
  _sema.release();
}

extern "C" void c_logger(char level, const char *file, uint32_t lineNbr,
                         const char *function, const char *fmt, ...) {
  if (level == 'D' || level == 'T') return;
  logger._sema.wait();
  if (logger._line == 0) {
    ::printf("%s:%d %s:%u\n", __FILE__, __LINE__, file, (unsigned int)lineNbr);
    //		_sema.release();
    return;
  }

  va_list args;
  va_start(args, fmt);
  static char logLine[256];
  vsnprintf(logLine, sizeof(logLine) - 1, fmt, args);
  va_end(args);

  extern void *pxCurrentTCB;
  //	if ( _application[0]==0)
  ::snprintf(logger._application, sizeof(logger._application), "%X",
             (uint32_t)pxCurrentTCB);
  *logger._line = stringFormat( "%+10.10s %c | %8s | %s | %15s:%4d | %s",
                logger._application, level, logger.time(), Sys::hostname(),
                file, lineNbr, logLine);
  logger.flush();
  logger._sema.release();
}

void Log::flush() {
  if (_logFunction) _logFunction((char *)_line->c_str(), _line->size());
  *_line = "";
}

void Log::level(LogLevel l) { _level = l; }

Log::LogLevel Log::level() { return _level; }

//_________________________________________ EMBEDDED

const char *Log::time() {
  static char szTime[20];
  snprintf(szTime, sizeof(szTime), "%llu", Sys::millis());
  return szTime;
}
