/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <SerialSpine.h>
#include <limero.h>
#include <stdio.h>

#include "LedBlinker.h"
#include "esp_spi_flash.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

Log logger(1024);
// ---------------------------------------------- THREAD
Thread thisThread("main");
Thread ledThread("led");
Thread mqttThread("mqtt");
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
Poller poller(mqttThread);

#include <UltraSonic.h>
Uext uextUs(2);
UltraSonic ultrasonic(thisThread, &uextUs);

extern "C" void app_main(void) {
#ifdef HOSTNAME
  Sys::hostname(S(HOSTNAME));
#endif

  SerialSpine spine(workerThread);
  spine.init();

  INFO("%s : %s ", systemHostname().c_str(), systemBuild().c_str());
  //-----------------------------------------------------------------  SYS props
  poller >> systemUptime >> spine.publisher<uint64_t>("system/uptime");
  poller >> systemHeap >> spine.publisher<uint32_t>("system/heap");
  poller >> systemHostname >> spine.publisher<std::string>("system/hostname");
  poller >> systemBuild >> spine.publisher<std::string>("system/build");
  poller >> systemAlive >> spine.publisher<bool>("system/alive");

  led.init();
  spine.connected >> led.blinkSlow;
  spine.connected >>
      [&](const bool &b) { INFO(" connected : %s", b ? "true" : "false"); };
  spine.connected >> poller.connected;
  //------------------------------------------------------------------- US
  ultrasonic.init();
  ultrasonic.distance >> spine.publisher<int32_t>("us/distance");

  ledThread.start();
  mqttThread.start();
  workerThread.start();
  thisThread.run();  // DON'T EXIT , local variable will be destroyed
}