#include <Log.h>
#include <Sys.h>
#include <string.h>
#include <unistd.h>

uint64_t Sys::_upTime;
char Sys::_hostname[30] = "";
uint64_t Sys::_boot_time = 0;

//_____________________________________________________________ LINUX and CYGWIN
#if defined(__linux__) || defined(__CYGWIN__) || defined(__APPLE__)
#include <time.h>

#include <chrono>

uint64_t Sys::millis() // time in msec since boot, only increasing
{
  using namespace std::chrono;
  milliseconds ms =
      duration_cast<milliseconds>(system_clock::now().time_since_epoch());
  Sys::_upTime =
      ms.count(); // system_clock::now().time_since_epoch().count() / 1000000;
  return _upTime;
}

void Sys::init() { gethostname(_hostname, sizeof(_hostname) - 1); }

void Sys::hostname(const char *hostname) {
  strncpy(_hostname, hostname, sizeof(_hostname));
}

const char *Sys::hostname() {
  if (_hostname[0] == 0)
    Sys::init();
  return _hostname;
}

void Sys::delay(uint32_t msec) {
  struct timespec ts;
  ts.tv_sec = msec / 1000;
  ts.tv_nsec = (msec - ts.tv_sec * 1000) * 1000000;
  nanosleep(&ts, NULL);
};

#endif

#include <Log.h>
#include <Sys.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdint.h>
#include <sys/time.h>
//#include <espressif/esp_wifi.h>

uint32_t Sys::getSerialId() {
  union {
    uint8_t my_id[6];
    uint32_t lsb;
  };
  esp_efuse_mac_get_default(my_id);
  //      esp_efuse_mac_get_custom(my_id);
  //   sdk_wifi_get_macaddr(STATION_IF, my_id);
  return lsb;
}

const char *Sys::getProcessor() { return "ESP8266"; }
const char *Sys::getBuild() { return __DATE__ " " __TIME__; }

uint32_t Sys::getFreeHeap() { return xPortGetFreeHeapSize(); };

const char *Sys::hostname() {
  if (_hostname[0] == 0) {
    Sys::init();
  }
  return _hostname;
}

void Sys::init() {
  std::string hn;
  union {
    uint8_t macBytes[6];
    uint16_t macInt[3];
  };
  if (esp_read_mac(macBytes, ESP_MAC_WIFI_STA) != ESP_OK)
    WARN(" esp_base_mac_addr_get() failed.");
  hn = "ESP32-";
  hn += std::to_string(macInt[2] );
  Sys::hostname(hn.c_str());
}

uint64_t Sys::millis() { return Sys::micros() / 1000; }

uint32_t Sys::sec() { return millis() / 1000; }

uint64_t Sys::micros() { return esp_timer_get_time(); }

uint64_t Sys::now() { return _boot_time + Sys::millis(); }

void Sys::setNow(uint64_t n) { _boot_time = n - Sys::millis(); }

void Sys::hostname(const char *h) { strncpy(_hostname, h, strlen(h) + 1); }

void Sys::setHostname(const char *h) { strncpy(_hostname, h, strlen(h) + 1); }

void Sys::delay(unsigned int delta) {
  uint32_t end = Sys::millis() + delta;
  while (Sys::millis() < end) {
  };
}

extern "C" uint64_t SysMillis() { return Sys::millis(); }
