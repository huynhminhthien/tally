#ifndef TALLY_h
#define TALLY_h

#include <ATEMbase.h>
#include <ATEMstd.h>
#include <ArduinoLog.h>
#include <Ethernet.h>
#include <SPI.h>
#include <SoftwareSerial.h>
#include <TimerOne.h>

#define ARRAY_SIZE(variable) (*(&variable + 1) - variable)

// define for pin number of switch
typedef enum device { ATEM = 3, VMIX = 4, ROLAND = 5 } TALLY_TYPE;

#define MAX_TALLY 8
#define rolandTX 6
#define rolandRX 7
#define CS_SPI 10
#define DEVICE_DEFAULT ATEM

typedef enum rolandTallyParam {
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

  ATEMstd _atem_switcher;
  EthernetClient _client;

  IPAddress _vmix_server;
  IPAddress _atem_server;
  uint16_t _port_vmix = 8099;  // Vmix fixed port 8099

  TALLY_TYPE _tally_type = DEVICE_DEFAULT;

  uint8_t _camera_status[MAX_TALLY] = {0};
  uint8_t _previous_roland[4] = {0};
  bool _preview_tally_previous[MAX_TALLY] = {true};
  bool _program_tally_previous[MAX_TALLY] = {true};

  Tally();
  bool HandleDataFromVmix(String data);
  void HandleDataFromAtem();
  void HandleDataFromRoland(String data);
  void ConnectToVmix();
  void InitVmix();
  void InitAtem();
  void InitRoland();
  void InitSwitchInput();
  void DumpStatusCamera();
  static void TimerIsr();

  static Tally* m_instance;
  static SoftwareSerial _roland;

 public:
  static Tally* Instance();

  void Begin();
  void InitConnectionWithServerSide();
  uint8_t* ProcessTally();
  void CheckConnection();
  void HandleSwitchDevice();
  TALLY_TYPE WhichDevice() { return _tally_type; }
};

#endif