#include <SoftwareSerial.h>

// Uncomment line below to fully disable logging
// #define DISABLE_LOGGING

#include "tally.h"

#define ARRAY_SIZE(variable) (*(&variable + 1) - variable)

typedef enum Button {
  kUp = 3,
  kDown = 4,
  kSet = 5,
  kMode = 12
} BUTTON_TYPE;
typedef enum Mode {
  kConfig = 0,
  kTallySwitch,
  kModeMax
} MODE_TYPE;

SoftwareSerial RF(8, 9);  // RX, TX

char send_data[MAX_TALLY + 2] = {0x30};
char* camera_status = nullptr;
const uint8_t button[] = {kUp, kDown, kSet, kMode};
uint8_t mode = kTallySwitch;
uint8_t tally_device = Tally::kVmix;

Device* my_device = nullptr;

void setup() {
  Serial.begin(115200);
  while (!Serial && !Serial.available()) {
    // wait for serial port to connect. Needed for native USB port only
  }
  Log.begin(LOG_LEVEL_VERBOSE, &Serial);

  // RF.begin(9600);
  Log.notice(F("Start" CR));
  // ButtonInit();

  // init start/stop value
  send_data[0] = 0x31;
  send_data[MAX_TALLY + 1] = 0x3B;

  my_device = Tally::Instance()->GetDevice((Tally::TYPE_DEVICE)tally_device);
}

void loop() {
  camera_status = my_device->HandleData();
  if (camera_status) {
    uint8_t len = strlen((const char*)camera_status);
    memcpy(send_data + 1, camera_status, len);
    send_data[len + 1] = 0x3B;

    for (uint8_t i = 0; i < ARRAY_SIZE(send_data); i++) {
      Log.notice("%d" CR, send_data[i]);
    }
    RF.println(send_data);
  }
  my_device->CheckConnection();
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
        case kMode:
          mode++;
          if (mode >= kModeMax) {
            mode = 0;
          }
          Log.trace(F("MODE button --> %s" CR),
                    mode == 0 ? "CONFIG" : "TALLY");
          break;
        case kUp:
          if (mode == kTallySwitch) {
            tally_device++;
            if (tally_device >= Tally::kDeviceMax) {
              tally_device = Tally::kDeviceMin + 1;
            }
            Log.trace(F("UP button - Tally mode --> %d" CR), tally_device);
          } else if (mode == kConfig) {
          }
          break;
        case kDown:
          if (mode == kTallySwitch) {
            tally_device--;
            if (tally_device == Tally::kDeviceMin) {
              tally_device = Tally::kDeviceMax - 1;
            }
            Log.trace(F("DOWN button - Tally mode --> %d" CR), tally_device);
          } else if (mode == kConfig) {
          }
          break;
        case kSet:
          Log.trace(F("SET button" CR));
          my_device =
              Tally::Instance()->GetDevice((Tally::TYPE_DEVICE)tally_device);
          break;
        default:
          Log.warning(F("button %d is not support" CR), button[i]);
          break;
      }
      return;
    }
  }
}
