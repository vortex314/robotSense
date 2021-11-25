/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <Hardware.h>
#include <Spine.h>
#include <limero.h>
#include <stdio.h>

#include "LedBlinker.h"
#include "esp_spi_flash.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include <driver/uart.h>

// ---------------------------------------------- THREAD
Thread thisThread("main");
Thread ledThread("led");
Thread spineThread("mqtt");
ThreadProperties props = {.name = "worker",
                          .stackSize = 5000,
                          .queueSize = 20,
                          .priority = 24 | portPRIVILEGE_BIT};
Thread workerThread(props);
#define PIN_LED 2

LedBlinker led(ledThread, PIN_LED, 100);

// ---------------------------------------------- system properties
LambdaSource<std::string> systemBuild([]() { return __DATE__ " " __TIME__; });
LambdaSource<std::string> systemHostname([]() { return Sys::hostname(); });
LambdaSource<uint32_t> systemHeap([]() { return Sys::getFreeHeap(); });
LambdaSource<uint64_t> systemUptime([]() { return Sys::millis(); });
LambdaSource<bool> systemAlive([]() { return true; });
Poller poller(spineThread);

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
#include <SerialFrame.h>
#include <Spine.h>
Spine spine(workerThread);
UART &uartUsb(UART::create(UART_NUM_0, 1, 3));
SerialFrame serialFrame(workerThread, uartUsb);

extern "C" void app_main(void) {
#ifdef HOSTNAME
  Sys::hostname(S(HOSTNAME));
#endif
  INFO("%s : %s ", systemHostname().c_str(), systemBuild().c_str());

  serialFrame.init();
  spine.init();
  led.init();

  serialFrame.rxdFrame >> spine.rxdFrame;
  spine.txdFrame >> serialFrame.txdFrame;
  spine.connected >> led.blinkSlow;
  spine.connected >> [&](const bool &b) {
    static bool prevState = false;
    if (b != prevState)
      INFO(" connected : %s", b ? "true" : "false");
    prevState = b;
  };

  //-----------------------------------------------------------------  SYS props
  spine.connected >> poller.connected;
  poller >> systemUptime >> spine.publisher<uint64_t>("system/uptime");
  poller >> systemHeap >> spine.publisher<uint32_t>("system/heap");
  poller >> systemHostname >> spine.publisher<std::string>("system/hostname");
  poller >> systemBuild >> spine.publisher<std::string>("system/build");
  poller >> systemAlive >> spine.publisher<bool>("system/alive");

  //------------------------------------------------------------------- US
  /*
  ultrasonic.init();
  ultrasonic.distance >> spine.publisher<int32_t>("us/distance");
*/
#ifdef GPS
  gps.init(); // no thread , driven from interrupt
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
    ledThread.start();
    spineThread.start();
    workerThread.start();
    thisThread.run(); // DON'T EXIT , local variable will be destroyed
  } catch (const char *msg) {
    ERROR("EXCEPTION => %s", msg);
    while (1) {
    };
  }
}
