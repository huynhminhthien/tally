#include "tally.h"

Tally* Tally::m_instance = nullptr;
Device* Tally::m_device = nullptr;

Tally* Tally::Instance() {
  if (!m_instance) {
    m_instance = new Tally();
  }
  return m_instance;
}

Device* Tally::GetDevice(TYPE_DEVICE type) {
  if (_current_device != type && type > kDeviceMin && type < kDeviceMax) {
    if (m_device) delete m_device;
    switch (type) {
      case kAtem:
        _current_device = kAtem;
        m_device = new Atem();
        break;
      case kVmix:
        _current_device = kVmix;
        m_device = new Vmix();
        break;
      case kRoland:
        _current_device = kRoland;
        m_device = new Roland();
        break;
      default:
        Log.error("device %s is not support", type);
        return nullptr;
    }
    m_device->Init();
  }
  return m_device;
}

void Device::SetMacAddress(uint8_t* mac_addr) {
  memcpy(_mac, mac_addr, sizeof(mac_addr));
}

void Device::SetIPAddress(uint8_t* ip_addr) {
  memcpy(_my_ip, ip_addr, sizeof(ip_addr));
}

void Device::InitEthernet() {
retry:
  // To configure the CS pin
  Ethernet.init(CS_SPI);
  // start the Ethernet
  Ethernet.begin(_mac, IPAddress(_my_ip[0], _my_ip[1], _my_ip[2], _my_ip[3]));

  // Check for Ethernet hardware present
  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    Log.error(
        F("Ethernet shield was not found. "
          "Sorry, can't run without hardware. :(" CR));
    delay(5000);
    goto retry;
  }
  Log.notice(F("Ethernet OK" CR));
  if (Ethernet.linkStatus() == LinkOFF) {
    Log.error(F("Ethernet cable is not connected." CR));
    goto retry;
  }

  IPAddress ip = Ethernet.localIP();
  Log.notice("My IP address: %d.%d.%d.%d" CR, ip[0], ip[1], ip[2], ip[3]);
}

Atem::Atem() : _atem_server{192, 168, 1, 10} {
  uint8_t mac[] = {0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x02};
  uint8_t ip[] = {192, 168, 1, 10};
  SetMacAddress(mac);
  SetIPAddress(ip);
  InitEthernet();
}

void Atem::Init() {
  // Initialize a connection to the switcher
  _atem_switcher.begin(IPAddress(_atem_server[0], _atem_server[1],
                                 _atem_server[2], _atem_server[3]));
  // set to 0x80 to enable debug
  _atem_switcher.serialOutput(0);
  _atem_switcher.connect();
}

char* Atem::HandleData() {
  /*
      Check for packets, respond to them etc. Keeping the connection alive!
      VERY important that this function is called all the time - otherwise
      connection might be lost because packets from the switcher is
      overlooked and not responded to.
      */
  _atem_switcher.runLoop();
  if (DataProcessing()) {
    return _camera_status;
  }
  return nullptr;
}

bool Atem::DataProcessing() {
  bool is_change = false;
  for (uint8_t tally_number = 1; tally_number <= MAX_TALLY; tally_number++) {
    bool program_tally = _atem_switcher.getProgramTally(tally_number);
    bool preview_tally = _atem_switcher.getPreviewTally(tally_number);

    if ((_program_tally_previous[tally_number - 1] != program_tally) ||
        (_preview_tally_previous[tally_number - 1] !=
         preview_tally)) {  // changed?
      if ((program_tally && !preview_tally) ||
          (program_tally &&
           preview_tally)) {  // only program, or program AND preview
        is_change = true;
        _camera_status[tally_number - 1] = 0x32;     // red
      } else if (preview_tally && !program_tally) {  // only preview
        is_change = true;
        _camera_status[tally_number - 1] = 0x31;      // green
      } else if (!preview_tally || !program_tally) {  // neither
        is_change = true;
        _camera_status[tally_number - 1] = 0x30;  // black
      }
    }

    _program_tally_previous[tally_number - 1] = program_tally;
    _preview_tally_previous[tally_number - 1] = preview_tally;
  }
  return is_change;
}

void Atem::CheckConnection() {
  // If connection is gone anyway, try to reconnect:
  if (_atem_switcher.isConnectionTimedOut()) {
    Log.warning(
        F("Connection to ATEM Switcher has timed out - reconnecting!"));
    _atem_switcher.connect();
  }
}

Vmix::Vmix() : _vmix_server{192, 168, 0, 100} {
  uint8_t mac[] = {0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x02};
  uint8_t ip[] = {192, 168, 0, 101};
  SetMacAddress(mac);
  SetIPAddress(ip);
  InitEthernet();
}

void Vmix::Init() {
  TryToConnect();
  _client.println(F("SUBSCRIBE TALLY"));
}

char* Vmix::HandleData() {
  uint8_t timeout = 0;
  const uint8_t max_timeout = 10;  // 1s
  while (!_client.available() && timeout < max_timeout) {
    delay(100);
    timeout++;
  }
  if (timeout == max_timeout) return nullptr;
  if (_client.available()) {
    String data = _client.readStringUntil('\r\n');
    if (DataProcessing(data)) {
      return _camera_status;
    }
  }
  return nullptr;
}

bool Vmix::DataProcessing(String data) {
  const String vmix_tally_rsp = "TALLY OK";
  uint8_t new_state = 0;
  uint8_t vmix_rsp_len = vmix_tally_rsp.length();
  uint8_t data_len = data.length() - 1;
  bool is_change = false;
  // Check if server data is Tally data
  if (data.indexOf(vmix_tally_rsp) == 0) {
    for (uint8_t tally_number = 1;
         tally_number <= MAX_TALLY && tally_number + vmix_rsp_len < data_len;
         tally_number++) {
      new_state = (uint8_t)data.charAt(tally_number + vmix_rsp_len);
      if (new_state != _camera_status[tally_number - 1]) {
        _camera_status[tally_number - 1] = new_state;
        is_change = true;
      }
    }
  }
  Log.notice("Response from vMix: %s" CR, data.c_str());

  return is_change;
}

void Vmix::CheckConnection() {
  if (!_client.connected()) {
    Log.notice(F("disconnected Vmix" CR));
    TryToConnect();
  }
}

void Vmix::TryToConnect() {
  uint8_t timeout = 0;
  const uint8_t max_timeout = 5;  // 1s
  while (!_client.connect(_vmix_server, _port_vmix) && timeout < max_timeout) {
    delay(200);
    timeout++;
  }
  if (timeout == max_timeout) return;
  Log.notice(F("connected Vmix" CR));
}

HardwareSerial Roland::_roland = HardwareSerial(2);

void Roland::RolandRequest() {
  // request to ROLAND->stxQPL : 8;
  const char roland_request[] = {0x02, 0x51, 0x50, 0x4C, 0x3A, 0x38, 0x3B};
  _roland.println(roland_request);
}

void Roland::Init() {
  _roland.begin(9600);
  _timer.attach(kRolandPeriod, RolandRequest);
}
char* Roland::HandleData() {
  String input_string = "";
  while (_roland.available()) {
    // get the new byte
    char inChar = (char)_roland.read();
    // add it to the input_string
    input_string += inChar;
    // ACK (06H)
    if (inChar == 0x06) {
      if (DataProcessing(input_string)) {
        return _camera_status;
      }
    }
  }
  return nullptr;
}

/**
 * @brief handle data from ROLAND
 *  stxQPL:b;
 *  Response command parameters
 *    when a=0, b: 0 (CH 1)–3 (CH 4) PGM
 *    When a=1, b: 0 (CH 1)–3 (CH 4) PST
 *    When a=2, b: 0 (OFF), 1 (ON) [PinP] button
 *    When a=3, b: 0 (OFF), 1 (ON) [SPLIT] button
 *    When a=4, b: 0 (OFF), 1 (ON) [DSK] button
 *    When a=5, b: 0 (WIPE), 1 (MIX), 2 (CUT) TRANSITION buttons
 *    When a=6, b: 0–255 Output fade level
 *                 0: black, 255: white, 128: center
 *    When a=7, b: 0–255 Output level of A/B fader
 *                 0: bus B end, 255: bus A end, 128: center
 *    When a=8, sends all information described above.
 *    Example: stxQPL:0,1,0,1,1,0,100,255;
 *
 * @param response
 */
bool Roland::DataProcessing(String response) {
  // stxQPL:
  const char roland_tally_rsp[] = {0x02, 0x51, 0x50, 0x4C, 0x3A};
  const uint8_t len = strlen(roland_tally_rsp);
  const char* content_rsp = response.c_str();
  // verify if response status of operation-panel buttons
  if (!strncmp(response.c_str(), roland_tally_rsp, len)) {
    char* pch;
    uint8_t infor_tally[kMaxParam] = {0};
    uint8_t i = 0;
    // split each number via ',;' character delimiter
    pch = strtok((char*)content_rsp + len, ",;");
    while (pch != nullptr && i < kMaxParam) {
      infor_tally[i] = atoi(pch);
      Log.notice("%d" CR, infor_tally[i]);
      i++;
      pch = strtok(nullptr, ",;");
    }
    // set all status tally to off
    memset(_camera_status, '0', MAX_ROLAND_CAMERA);
    _camera_status[MAX_ROLAND_CAMERA] = '\0';
    // assign PGM LED and PST LED
    _camera_status[infor_tally[kPGM]] = 0x32;  // red
    _camera_status[infor_tally[kPST]] = 0x31;  // green
  }
  bool is_change = false;
  for (uint8_t i = 0; i < MAX_ROLAND_CAMERA; i++) {
    if (_camera_status[i] != _previous_roland[i]) is_change = true;
  }
  if (is_change) memcpy(_previous_roland, _camera_status, MAX_ROLAND_CAMERA);
  return is_change;
}

void Roland::CheckConnection() {}