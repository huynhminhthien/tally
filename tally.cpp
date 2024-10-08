#include "tally.h"

Tally* Tally::m_instance = nullptr;
SoftwareSerial Tally::_roland = SoftwareSerial(rolandRX, rolandTX);

Tally* Tally::Instance() {
  if (!m_instance) {
    m_instance = new Tally();
  }
  return m_instance;
}

Tally::Tally()
    : _mac{0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x02},
      _ip(192, 168, 0, 177),
      _vmix_server(192, 168, 0, 100),
      _atem_server(192, 168, 0, 100) {}

void Tally::Begin() {
  InitSwitchInput();
retry:
  // To configure the CS pin
  Ethernet.init(CS_SPI);
  // start the Ethernet
  Ethernet.begin(_mac, _ip);

  // Check for Ethernet hardware present
  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    Log.error(
        "Ethernet shield was not found. "
        "Sorry, can't run without hardware. :(" CR);
    while (true) {
      delay(1);  // do nothing, no point running without Ethernet hardware
    }
  }
  if (Ethernet.linkStatus() == LinkOFF) {
    Log.error("Ethernet cable is not connected." CR);
    goto retry;
  }

  IPAddress ip = Ethernet.localIP();
  Log.notice("My IP address: %s.%s.%s.%s" CR, ip[0], ip[1], ip[2], ip[3]);
}

uint8_t* Tally::ProcessTally() {
  bool is_change = false;
  switch (_tally_type) {
    case VMIX:
      while (_client.available()) {
        String data = _client.readStringUntil('\r\n');
        is_change = HandleDataFromVmix(data);
        if (is_change) {
          for (uint8_t i = 0; i < MAX_TALLY; i++) {
            Log.notice("%d" CR, _camera_status[i]);
          }
        }
      }
      // if (_client.available()) {
      //   char c = client.read();
      //   Serial.print(c);
      // }
      break;
    case ATEM:
      // Check for packets, respond to them etc. Keeping the connection alive!
      // VERY important that this function is called all the time - otherwise
      // connection might be lost because packets from the switcher is
      // overlooked and not responded to.
      _atem_switcher.runLoop();
      HandleDataFromAtem();
      break;
    case ROLAND:
      String input_string = "";
      while (_roland.available()) {
        // get the new byte
        char inChar = (char)_roland.read();
        // add it to the input_string
        input_string += inChar;
        // ACK (06H)
        if (inChar == 0x06) {
          HandleDataFromRoland(input_string);
          is_change = true;
        }
      }
      break;
    default:
      Log.error("device not support (%d)" CR, _tally_type);
      break;
  }
  return (is_change) ? _camera_status : nullptr;
}

void Tally::CheckConnection() {
  switch (_tally_type) {
    case VMIX:
      if (!_client.connected()) {
        Log.notice("disconnected Vmix" CR);
        ConnectToVmix();
      }
      break;
    case ATEM:
      // // If connection is gone anyway, try to reconnect:
      // if (_atem_switcher.isConnectionTimedOut()) {
      //   Log.warning(
      //       "Connection to ATEM Switcher has timed out - reconnecting!");
      //   _atem_switcher.connect();
      // }
      break;
    case ROLAND:
      break;
    default:
      Log.error("device not support (%d)" CR, _tally_type);
      break;
  }
}

bool Tally::HandleDataFromVmix(String data) {
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

void Tally::HandleDataFromAtem() {
  for (uint8_t tally_number = 1; tally_number <= MAX_TALLY; tally_number++) {
    bool program_tally = _atem_switcher.getProgramTally(tally_number);
    bool preview_tally = _atem_switcher.getPreviewTally(tally_number);

    if ((_program_tally_previous[tally_number - 1] != program_tally) ||
        (_preview_tally_previous[tally_number - 1] !=
         preview_tally)) {  // changed?
      if ((program_tally && !preview_tally) ||
          (program_tally &&
           preview_tally)) {  // only program, or program AND preview
        _camera_status[tally_number - 1] = 0x32;      // red
      } else if (preview_tally && !program_tally) {   // only preview
        _camera_status[tally_number - 1] = 0x31;      // green
      } else if (!preview_tally || !program_tally) {  // neither
        _camera_status[tally_number - 1] = 0x30;      // black
      }
    }

    _program_tally_previous[tally_number - 1] = program_tally;
    _preview_tally_previous[tally_number - 1] = preview_tally;
  }
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
void Tally::HandleDataFromRoland(String response) {
  // stxQPL:
  const char roland_tally_rsp[] = {0x02, 0x51, 0x50, 0x4C, 0x3A};
  const uint8_t len = strlen(roland_tally_rsp);
  const char* content_rsp = response.c_str();
  // verify if response status of operation-panel buttons
  if (!strncmp(response.c_str(), roland_tally_rsp, len)) {
    char* pch;
    uint8_t infor_tally[MAXPARAM] = {0};
    uint8_t i = 0;
    // split each number via ',;' character delimiter
    pch = strtok(content_rsp + len, ",;");
    while (pch != nullptr && i < MAXPARAM) {
      infor_tally[i] = atoi(pch);
      Log.notice("%d" CR, infor_tally[i]);
      i++;
      pch = strtok(nullptr, ",;");
    }
    // set all status tally to off
    memset(_camera_status, '0', MAX_TALLY);
    // assign PGM LED and PST LED
    _camera_status[infor_tally[PGM]] = 0x32;  // red
    _camera_status[infor_tally[PST]] = 0x31;  // green
    DumpStatusCamera();
  }
}

/**
 * @brief dump status of each tally, using for test purpose
 *
 */
void Tally::DumpStatusCamera() {
  Log.notice("status camera" CR);
  for (uint8_t i = 0; i < MAX_TALLY; i++) {
    Log.notice("%d" CR, _camera_status[i]);
  }
}

void Tally::InitConnectionWithServerSide() {
  switch (_tally_type) {
    case VMIX:
      InitVmix();
      break;
    case ATEM:
      InitAtem();
      break;
    case ROLAND:
      InitRoland();
      break;
    default:
      Log.error("device not support (%d)" CR, _tally_type);
      break;
  }
}

void Tally::InitVmix() {
  ConnectToVmix();
  _client.println("SUBSCRIBE TALLY");
}

void Tally::InitAtem() {
  // Initialize a connection to the switcher
  _atem_switcher.begin(_atem_server);
  // set to 0x80 to enable debug
  _atem_switcher.serialOutput(0);
  _atem_switcher.connect();
}

static void Tally::TimerIsr() {
  // request to ROLAND->stxQPL : 8;
  const char roland_request[] = {0x02, 0x51, 0x50, 0x4C, 0x3A, 0x38, 0x3B};
  _roland.println(roland_request);
}

void Tally::InitRoland() {
  _roland.begin(9600);
  Timer1.initialize(300000);
  Timer1.attachInterrupt(TimerIsr);
  Timer1.start();
}

void Tally::InitSwitchInput() {
  pinMode(ATEM, INPUT);
  pinMode(VMIX, INPUT);
  pinMode(ROLAND, INPUT);
}

void Tally::HandleSwitchDevice() {
  uint8_t device[] = {ATEM, VMIX, ROLAND};
  for (uint8_t i = 0; i < ARRAY_SIZE(device); i++) {
    if (!digitalRead(device[i])) {
      if (_tally_type != device[i]) {
        _tally_type = device[i];
        InitConnectionWithServerSide();
      }
      break;
    }
  }
  if (_tally_type != ROLAND) {
    Timer1.stop();
  }
}

void Tally::ConnectToVmix() {
  while (!_client.connect(_vmix_server, _port_vmix)) {
    delay(1000);
    Log.notice("." CR);
  }
  Log.notice("connected Vmix" CR);
}
