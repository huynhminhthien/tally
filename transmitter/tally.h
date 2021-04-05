#ifndef TALLY_h
#define TALLY_h

#include <ATEM.h>
#include <ArduinoLog.h>
#include <HardwareSerial.h>
#include <Ticker.h>

#define ARRAY_SIZE(variable) (*(&variable + 1) - variable)

// define for pin number of switch
typedef enum Device {
  E_TALLY_MIN = 0,
  E_ATEM,
  E_VMIX,
  E_ROLAND,
  E_TALLY_MAX
} TALLY_TYPE;

#define MAX_TALLY 8
#define ROLAND_TX 6
#define ROLAND_RX 7
#define CS_SPI 10
#define DEVICE_DEFAULT E_ATEM

typedef enum RolandTallyParam {
  PGM,
  PST,
  PinP,
  SPLIT,
  DSK,
  TRANSITION,
  FADE,
  FADER,
  MAXPARAM
} ROLAND_PARAM;

class Tally {
 private:
  // MAC address must be unique in LAN
  byte _mac[6];
  IPAddress _ip;

  ATEM _atem_switcher;
  EthernetClient _client;

  IPAddress _vmix_server;
  IPAddress _atem_server;
  uint16_t _port_vmix = 8099;  // Vmix fixed port 8099

  TALLY_TYPE _tally_type = DEVICE_DEFAULT;

  uint8_t _camera_status[MAX_TALLY] = {0};
  uint8_t _previous_roland[4] = {0};
  bool _preview_tally_previous[MAX_TALLY] = {true};
  bool _program_tally_previous[MAX_TALLY] = {true};

  const float kRolandPeriod = 0.5;  //seconds

  Tally();
  bool HandleDataFromVmix(String data);
  void HandleDataFromAtem();
  void HandleDataFromRoland(String data);
  void ConnectToVmix();
  void InitVmix();
  void InitAtem();
  void InitRoland();
  void DumpStatusCamera();
  static void RolandRequest();

  static Tally* m_instance;
  static HardwareSerial _roland;
  Ticker _timer;

 public:
  static Tally* Instance();

  void Begin();
  void InitConnectionWithServerSide();
  uint8_t* ProcessTally();
  void CheckConnection();
  void SetTallyDevice(TALLY_TYPE type);
  TALLY_TYPE WhichDevice() { return _tally_type; }
};

#endif