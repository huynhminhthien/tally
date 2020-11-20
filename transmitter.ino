#include <ArduinoLog.h>
#include <SoftwareSerial.h>

// Uncomment line below to fully disable logging
// #define DISABLE_LOGGING

#include "tally.h"

SoftwareSerial RF(8, 9);  // RX, TX

uint8_t send_data[MAX_TALLY + 2] = {0x30};
uint8_t *camera_status = nullptr;

void setup() {
  Serial.begin(115200);
  while (!Serial && !Serial.available()) {
    // wait for serial port to connect. Needed for native USB port only
  }
  Log.begin(LOG_LEVEL_VERBOSE, &Serial);

  RF.begin(9600);
  Log.notice("Start" CR);

  // init start/stop value
  send_data[0] = 0x31;
  send_data[MAX_TALLY + 1] = 0x3B;

  // start tally
  Tally::Instance()->Begin();
  Tally::Instance()->InitConnectionWithServerSide();
}

void loop() {
  camera_status = Tally::Instance()->ProcessTally();
  if (camera_status) {
    uint8_t len = (Tally::Instance()->WhichDevice() != ROLAND &&
                   strlen(camera_status) <= MAX_TALLY)
                      ? strlen(camera_status)
                      : 4;
    memcpy(send_data + 1, camera_status, len);

    for (uint8_t i = 0; i < ARRAY_SIZE(send_data); i++) {
      Log.notice("%d" CR, send_data[i]);
    }
    RF.println((const char)send_data);
  }
  Tally::Instance()->CheckConnection();
  Tally::Instance()->HandleSwitchDevice();
}
