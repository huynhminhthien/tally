#include <ArduinoLog.h>
#include <SoftwareSerial.h>

// Uncomment line below to fully disable logging
// #define DISABLE_LOGGING

#include "tally.h"

typedef enum Button { E_UP = 3, E_DOWN = 4, E_SET = 5, E_MODE = 6 } BUTTON_TYPE;
typedef enum Mode { E_CONFIG = 0, E_TALLY_SWITCH, E_MODE_MAX } MODE_TYPE;

SoftwareSerial RF(8, 9);  // RX, TX

uint8_t send_data[MAX_TALLY + 2] = {0x30};
uint8_t *camera_status = nullptr;
const uint8_t button[] = {E_UP, E_DOWN, E_SET, E_MODE};
uint8_t mode = E_TALLY_SWITCH;
uint8_t tally_device = DEVICE_DEFAULT;

void setup() {
  Serial.begin(115200);
  while (!Serial && !Serial.available()) {
    // wait for serial port to connect. Needed for native USB port only
  }
  Log.begin(LOG_LEVEL_VERBOSE, &Serial);

  RF.begin(9600);
  Log.notice("Start" CR);
  ButtonInit();

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
    uint8_t len = (Tally::Instance()->WhichDevice() != E_ROLAND &&
                   strlen(camera_status) <= MAX_TALLY)
                      ? strlen(camera_status)
                      : 4;
    memcpy(send_data + 1, camera_status, len);
    send_data[len + 1] = 0x3B;

    for (uint8_t i = 0; i < ARRAY_SIZE(send_data); i++) {
      Log.notice("%d" CR, send_data[i]);
    }
    RF.println((const char)send_data);
  }
  Tally::Instance()->CheckConnection();
}

void ButtonInit() {
  // init external interrupt at pin 2
  pinMode(2, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(2), ButtonHandle, FALLING);
  for (uint8_t i = 0; i < ARRAY_SIZE(button); i++) {
    pinMode(button[i], INPUT);
  }
}

void ButtonHandle() {
  for (uint8_t i = 0; i < ARRAY_SIZE(button); i++) {
    if (!digitalRead(button[i])) {
      switch (button[i]) {
        case E_MODE:
          mode++;
          if (mode >= E_MODE_MAX) {
            mode = 0;
          }
          Log.trace("MODE button --> %s" CR, mode == 0 ? "CONFIG" : "TALLY");
          break;
        case E_UP:
          if (mode == E_TALLY_SWITCH) {
            tally_device++;
            if (tally_device >= E_TALLY_MAX) {
              tally_device = E_TALLY_MIN + 1;
            }
            Log.trace("UP button - Tally mode --> %d" CR, tally_device);
          } else if (mode == E_CONFIG) {
          }
          break;
        case E_DOWN:
          if (mode == E_TALLY_SWITCH) {
            tally_device--;
            if (tally_device == E_TALLY_MIN) {
              tally_device = E_TALLY_MAX - 1;
            }
            Log.trace("DOWN button - Tally mode --> %d" CR, tally_device);
          } else if (mode == E_CONFIG) {
          }
          break;
        case E_SET:
          Log.trace("SET button" CR);
          Tally::Instance()->SetTallyDevice(tally_device);
          break;
        default:
          Log.warning("button %d is not support" CR, button[i]);
          break;
      }
      return;
    }
  }
}
