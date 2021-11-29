/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <Hardware.h>
#include <Spine.h>
#include <driver/uart.h>
#include <limero.h>
#include <stdio.h>

#include "LedBlinker.h"
#include "esp_spi_flash.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

// ---------------------------------------------- THREAD
Thread thisThread("mainThread");
Thread spineThread("spineThread");
ThreadProperties props = {.name = "workerThread",
                          .stackSize = 5000,
                          .queueSize = 20,
                          .priority = 24 | portPRIVILEGE_BIT};
Thread workerThread(props);
#define PIN_LED 2

LedBlinker led(workerThread, PIN_LED, 100);

// ---------------------------------------------- system properties
LambdaSource<std::string> systemBuild([]() { return __DATE__ " " __TIME__; });
LambdaSource<std::string> systemHostname([]() { return Sys::hostname(); });
LambdaSource<uint32_t> systemHeap([]() { return Sys::getFreeHeap(); });
LambdaSource<uint64_t> systemUptime([]() { return Sys::millis(); });
LambdaSource<bool> systemAlive([]() { return true; });
Poller poller(spineThread);
ValueFlow<double> current;

Uext uext1(1);
ADC &adcPwr = uext1.getADC(LP_RXD);

/*
#include <UltraSonic.h>
Uext uextUs(2);
UltraSonic ultrasonic(workerThread, uextUs);

#ifdef GPS
#include <Neo6m.h>
Uext uext1(1);
Neo6m gps(thisThread, &uext1);
#endif
*/

//#define SERIAL_SPINE

#ifdef SERIAL_SPINE
#include <SerialFrame.h>
#include <Spine.h>

Spine spine(workerThread);
UART &uartUsb(UART::create(UART_NUM_0, 1, 3));
SerialFrame serialFrame(workerThread, uartUsb);

void initSpine() {
  spine.init();
  serialFrame.init();
  Log::setLogWriter(uartSendBytes);
  serialFrame.rxdFrame >> spine.rxdFrame;
  spine.txdFrame >> serialFrame.txdFrame;
}
#else
#include <UdpFrame.h>
#include <Wifi.h>

#include "esp_system.h"
#include "esp_wifi.h"
#include "nvs.h"
#include "nvs_flash.h"
Spine spine(workerThread);
Wifi wifi(workerThread);
UdpFrame udpFrame(workerThread);
nvs_handle _nvs = 0;

void initSpine() {
  if (_nvs != 0) return;
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
      err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }

  spine.init();
  wifi.init();
  udpFrame.init();
  wifi.connected >> udpFrame.online;
  udpFrame.rxdFrame >> spine.rxdFrame;
  spine.txdFrame >> udpFrame.txdFrame;
  udpFrame.start();
}

#endif
#include <math.h>
ThreadProperties adcThreadProps = {.name = "adcThread",
                                   .stackSize = 5000,
                                   .queueSize = 3,
                                   .priority = tskIDLE_PRIORITY + 1};

Thread adcThread(adcThreadProps);

double adcAvg = 0;
double adcSpread = 0;
#define MAX_ADC_VALUE 4095
#define BUCKET_COUNT 1024
#define BUCKET_SIZE (MAX_ADC_VALUE / BUCKET_COUNT)

uint32_t buckets[BUCKET_COUNT];
uint32_t measurementCount = 0;

void clearBuckets() {
  for (uint32_t i = 0; i < BUCKET_COUNT; i++) buckets[i] = 0;
  measurementCount = 0;
}

void addBucket(uint16_t value) {
  // scale 0->1023 in buckets
  int bucketIdx = value / BUCKET_SIZE;
  buckets[bucketIdx]++;
  measurementCount++;
}
void getBiggestBucket() {
  uint32_t size = 0;
  uint32_t bucketIdx = 0;
  double weightedSum = 0;
  int delta = 0;
  for (uint32_t i = 0; i < BUCKET_COUNT; i++) {
    if (size < buckets[i]) {
      size = buckets[i];
      bucketIdx = i;
    };
    delta = (i * BUCKET_SIZE) - 1935;
    weightedSum += abs(delta) * buckets[i];
  }
  int v = bucketIdx * BUCKET_SIZE;
  //  current = abs(v - 1935) / 0.3;
  current = weightedSum / measurementCount;
  INFO(" big %d : val : %d<->%d cnt : %d tot : %d abs : %f ", bucketIdx, v,
       v + BUCKET_SIZE, buckets[bucketIdx], measurementCount, current);

  // current : 1/1000 => 180 Ohm
  // 1A => 1mA => 180mV
  // scale == 4095 for 2450mV =>  1Bit == 2450 mv / 4096 = 0.6 mV
  // 1 A = 180/0.6 = 300 binary per A
  // 1935 zero value
  // 5A => 1500 ==> 1935 + 1500 = 3400
}

void adcRun() {
  INFO("start");
  TimerSource *adcTimer =
      new TimerSource(adcThread, 2000, true, "receiverTimer");
  *adcTimer >> [&](const TimerMsg &) {
    clearBuckets();
    uint64_t endTime = Sys::micros() + 1000000;
    while (Sys::micros() < endTime) {
      uint32_t value = adcPwr.getValue();
      addBucket(value);
    }
    getBiggestBucket();
  };
  INFO("kickoff thread");
  adcThread.start();
}

extern "C" void app_main(void) {
#ifdef HOSTNAME
  Sys::hostname(S(HOSTNAME));
#endif
  INFO("%s : %s ", systemHostname().c_str(), systemBuild().c_str());

  adcPwr.init();
  led.init();
  initSpine();
  adcRun();
  spine.connected >> led.blinkSlow;
  spine.connected >> [&](const bool &b) {
    static bool prevState = false;
    if (b != prevState) INFO(" connected : %s", b ? "true" : "false");
    prevState = b;
  };
  // spine.txdFrame >> [&](const Bytes &bs) { INFO("txd %d ", bs.size()); };

  //-----------------------------------------------------------------  SYS props
  spine.connected >> poller.connected;
  poller.interval = 1000;
  poller >> systemUptime >> spine.publisher<uint64_t>("system/uptime");
  poller >> systemHeap >> spine.publisher<uint32_t>("system/heap");
  poller >> systemHostname >> spine.publisher<std::string>("system/hostname");
  poller >> systemBuild >> spine.publisher<std::string>("system/build");
  poller >> systemAlive >> spine.publisher<bool>("system/alive");
  current >> spine.publisher<double>("power/current");
  current >> [&](const double &v) { INFO(" current : %f", v); };
  //------------------------------------------------------------------- US
  /*
  ultrasonic.init();
  ultrasonic.distance >> spine.publisher<int32_t>("us/distance");
*/
#ifdef GPS
  gps.init();  // no thread , driven from interrupt
  gps.location >>
      new LambdaFlow<Location, Json>([&](Json &json, const Location &loc) {
        json["lon"] = loc.longitude;
        json["lat"] = loc.latitude;
        return true;
      }) >>
      spine.publisher<Json>("gps/location");

  gps.satellitesInView >> spine.publisher<int>("gps/satellitesInView");
  gps.satellitesInUse >> spine.publisher<int>("gps/satellitesInUse");
#endif
  try {
    spineThread.start();
    workerThread.start();
    thisThread.run();  // DON'T EXIT , local variable will be destroyed
  } catch (const char *msg) {
    ERROR("EXCEPTION => %s", msg);
    while (1) {
    };
  }
}
