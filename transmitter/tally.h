/*

  Pinout/connections:
  W5500  | ESP32
  --------------------
  GND    | GND
  3.3V   | 3.3V
  MISO   | GPIO19
  MOSI   | GPIO23
  SCS    | GPIO5
  SCLK   | GPIO18

 */

#ifndef TALLY_h
#define TALLY_h

#include <ATEM.h>
#include <Ethernet.h>
#include <ArduinoLog.h>
#include <HardwareSerial.h>
#include <Ticker.h>

#define MAX_TALLY 8
#define MAX_ROLAND_CAMERA 4
#define CS_SPI 5

class Device {
 public:
  virtual void Init() = 0;
  virtual char* HandleData() = 0;
  virtual void CheckConnection() = 0;

 protected:
  // MAC address must be unique in LAN
  uint8_t _mac[6];
  uint8_t _my_ip[4];

  void InitEthernet();
  void SetMacAddress(uint8_t* mac_addr);
  void SetIPAddress(uint8_t* ip_addr);
};

class Tally {
 public:
  typedef enum TypeDevice {
    kDeviceMin = 0,
    kAtem,
    kVmix,
    kRoland,
    kDeviceMax
  } TYPE_DEVICE;

  static Tally* Instance();

  Device* GetDevice(TYPE_DEVICE type);
  TYPE_DEVICE WhichDevice() { return _current_device; }

 private:
  TYPE_DEVICE _current_device;
  static Tally* m_instance;
  static Device* m_device;

  Tally() : _current_device{kDeviceMin} {}
  ~Tally() {
    if (m_device) delete m_device;
    if (m_instance) delete m_instance;
  }
};

class Atem : public Device {
 public:
  Atem();
  ~Atem();
  void Init();
  char* HandleData();
  void CheckConnection();

 private:
  uint8_t _atem_server[4];
  ATEM _atem_switcher;
  char _camera_status[MAX_TALLY] = {0};
  bool _preview_tally_previous[MAX_TALLY] = {true};
  bool _program_tally_previous[MAX_TALLY] = {true};
  bool DataProcessing();
};

class Vmix : public Device {
 public:
  Vmix();
  ~Vmix();
  void Init();
  char* HandleData();
  void CheckConnection();

 private:
  uint8_t _vmix_server[4];
  char _camera_status[MAX_TALLY] = {0};
  uint16_t _port_vmix = 8099;  // Vmix fixed port 8099
  EthernetClient _client;

  void TryToConnect();
  bool DataProcessing(String data);
};

class Roland : public Device {
 public:
  Roland() : _previous_roland{0}, _camera_status{0} {}
  ~Roland() {}
  void Init();
  char* HandleData();
  void CheckConnection();

 private:
  static HardwareSerial _roland;
  Ticker _timer;
  const float kRolandPeriod = 0.5;  // seconds
  char _previous_roland[MAX_ROLAND_CAMERA];
  char _camera_status[MAX_ROLAND_CAMERA];
  typedef enum RolandTallyParam {
    kPGM,
    kPST,
    kPinP,
    kSplit,
    kDSK,
    kTransition,
    kFade,
    kFader,
    kMaxParam
  } ROLAND_PARAM;

  static void RolandRequest();
  bool DataProcessing(String response);
};
#endif
