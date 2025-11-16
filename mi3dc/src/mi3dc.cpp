#include "mi3dc.h"

// Mi3 Dash Button ist aktiv-low (zieht Leitung gegen GND)
#ifndef MI3DC_BTN_ACTIVE_LOW
  #define MI3DC_BTN_ACTIVE_LOW 1
#endif

// Ein globales Objekt, damit man im Sketch direkt "Mi3" nutzen kann
Mi3DC Mi3;

// Kleiner CRC Helper
// Protokoll-Variante wie bei vielen M365 / Mi Implementierungen:
// sum = LEN + alle DATA-Bytes, dann XOR 0xFFFF
static uint16_t mi3_crc16(uint8_t len, const uint8_t* data, uint8_t count)
{
  uint32_t sum = len;
  for (uint8_t i = 0; i < count; ++i) {
    sum += data[i];
  }
  uint16_t crc = (uint16_t)sum ^ 0xFFFF;
  return crc;
}

Mi3DC::Mi3DC()
: m_bus(nullptr),
  m_rxPin(9),
  m_txPin(8),
  m_btnPin(10),
  m_btnActiveLow(MI3DC_BTN_ACTIVE_LOW),
  m_started(false),
  m_throttleIdx(-1),
  m_brakeIdx(-1),
  m_len(0),
  m_idx(0),
  m_crcLo(0),
  m_state(WAIT_HEADER1)
{
  m_last.throttle     = 0;
  m_last.brake        = 0;
  m_last.button       = false;
  m_last.lastUpdateMs = 0;
}

void Mi3DC::begin(HardwareSerial &bus,
                  int rxPin,
                  int txPin,
                  int btnPin,
                  uint32_t baud)
{
  m_bus = &bus;
  m_rxPin = rxPin;
  m_txPin = txPin;
  m_btnPin = btnPin;
  m_btnActiveLow = MI3DC_BTN_ACTIVE_LOW;

  // Button-Pin vorbereiten (internes Pullup, weil aktiv-low)
  if (m_btnPin >= 0) {
    pinMode(m_btnPin, m_btnActiveLow ? INPUT_PULLUP : INPUT);
  }

  // Mi3 UART anschmeißen
  m_bus->begin(baud, SERIAL_8N1, m_rxPin, m_txPin);
  m_started = true;
  resetParser();
}

void Mi3DC::setControlIndices(uint8_t throttleIdx, uint8_t brakeIdx)
{
  // Einfach die Byte-Positionen merken, an denen das Dash Gas/Bremse rausgibt
  m_throttleIdx = (int8_t)throttleIdx;
  m_brakeIdx    = (int8_t)brakeIdx;
}

void Mi3DC::resetParser()
{
  // Parser geht wieder in den "warte auf 0x55" Zustand
  m_state = WAIT_HEADER1;
  m_len   = 0;
  m_idx   = 0;
  m_crcLo = 0;
}

void Mi3DC::poll()
{
  if (!m_started || !m_bus) return;

  // Alle verfügbaren Bytes vom Mi3-Bus einsammeln
  while (m_bus->available() > 0) {
    uint8_t b = static_cast<uint8_t>(m_bus->read());
    handleByte(b);
  }

  // Button-Status einlesen und schon hübsch als bool ablegen
  if (m_btnPin >= 0) {
    bool raw = digitalRead(m_btnPin);
    m_last.button = m_btnActiveLow ? (raw == LOW) : (raw == HIGH);
  }
}

Mi3DashData Mi3DC::getDashData() const
{
  return m_last;
}

// State-Machine für das 0x55 0xAA Frame-Parsing
void Mi3DC::handleByte(uint8_t b)
{
  switch (m_state) {
    case WAIT_HEADER1:
      // wir warten auf das erste 0x55
      if (b == 0x55) {
        m_state = WAIT_HEADER2;
      }
      break;

    case WAIT_HEADER2:
      // direkt nach 0x55 muss 0xAA kommen, sonst war es nur Noise
      if (b == 0xAA) {
        m_state = READ_LEN;
      } else if (b == 0x55) {
        // erneutes 0x55 → bleib in "warte auf zweites Byte"
        m_state = WAIT_HEADER2;
      } else {
        m_state = WAIT_HEADER1;
      }
      break;

    case READ_LEN:
      // LEN gibt an, wie viele Datenbytes (ab cmd) folgen
      m_len = b;
      // wir erwarten LEN+1 Bytes (dest+cmd+...)
      if (m_len < 2 || (uint16_t)m_len + 1 > MAX_FRAME) {
        // zu kurz / zu lang → verwerfen
        resetParser();
      } else {
        m_idx   = 0;
        m_state = READ_DATA;
      }
      break;

    case READ_DATA:
      // Daten nach LEN einsammeln (dest, cmd, sub, ...)
      if (m_idx < (uint8_t)(m_len + 1)) {
        m_buf[m_idx++] = b;
        if (m_idx >= (uint8_t)(m_len + 1)) {
          // genug Daten → jetzt kommt CRC
          m_state = READ_CK1;
        }
      } else {
        // Overflow → Parser neu starten
        resetParser();
      }
      break;

    case READ_CK1:
      // erstes CRC-Byte merken
      m_crcLo = b;
      m_state = READ_CK2;
      break;

    case READ_CK2: {
      // zweites CRC-Byte + Vergleich
      uint8_t crcHi = b;
      uint16_t crcRx = (uint16_t)m_crcLo | ((uint16_t)crcHi << 8);

      uint16_t crcCalc = mi3_crc16(m_len, m_buf, (uint8_t)(m_len + 1));

      if (crcRx == crcCalc) {
        // CRC passt → Frame an Auswerter weiterreichen
        handleFrame(m_buf, (uint8_t)(m_len + 1));
      }
      // Egal ob ok oder nicht: neuer Frame
      resetParser();
    } break;
  }
}

// Hier landet jedes gültige Frame (CRC ok)
void Mi3DC::handleFrame(const uint8_t *data, uint8_t len)
{
  if (len < 3) return;

  uint8_t dest = data[0];
  uint8_t cmd  = data[1];

  // Dash → ESC Controls: 0x20 0x65 / 0x20 0x64 (Mi3-Style)
  if (dest == 0x20 && (cmd == 0x65 || cmd == 0x64)) {
    // Nur auswerten, wenn wir die Indizes kennen
    if (m_throttleIdx >= 0 && (uint8_t)m_throttleIdx < len) {
      m_last.throttle = data[m_throttleIdx];
    }
    if (m_brakeIdx >= 0 && (uint8_t)m_brakeIdx < len) {
      m_last.brake = data[m_brakeIdx];
    }
    m_last.lastUpdateMs = millis();
  }
}

// ESC → Dash: Statusframe bauen und rausschicken
//
// Layout:
// 55 AA  L  21 64 00  drive  batt  N  s1  speed  error  ck0  ck1
//   |     |  |   |   |      |      |  |   |      |
//  hdr   LEN dst cmd sub   D      L  N   s1     speed/error
//
// - D      = driveMode (1..3, frei definierbar)
// - L      = battPercent (0..100 → kannst du dir im Kopf auf Balken mappen)
// - N      = Night/Headlight (0x00 = aus, 0x64 = an)
// - error  = komplettes Fehler-Byte (du entscheidest, was das bedeutet)
void Mi3DC::send2dash(uint8_t battPercent,
                      uint8_t speedKmh,
                      uint8_t driveMode,
                      bool    lightOn,
                      uint8_t errorCode)
{
  if (!m_started || !m_bus) return;

  const uint8_t dest = 0x21;  // ESC → Dash
  const uint8_t cmd  = 0x64;  // Status-Kanal
  const uint8_t sub  = 0x00;

  // Night-Byte wie bei M365:
  //  0x00 = nichts
  //  0x64 = Nacht / Licht an
  uint8_t N     = lightOn ? 0x64 : 0x00;
  uint8_t s1    = 0x00;       // Reserve, aktuell ungenutzt
  uint8_t error = errorCode;  // volles Fehlerbyte

  // DATA = [dest, cmd, sub, drive, batt, N, s1, speed, error]
  uint8_t data[9];
  uint8_t p = 0;
  data[p++] = dest;
  data[p++] = cmd;
  data[p++] = sub;
  data[p++] = driveMode;
  data[p++] = battPercent;
  data[p++] = N;
  data[p++] = s1;
  data[p++] = speedKmh;
  data[p++] = error;

  // LEN = Anzahl Bytes von cmd (0x64) bis inkl. letztes Payload-Byte
  //      (cmd, sub, drive, batt, N, s1, speed, error) = 8
  uint8_t len = 8;

  // CRC16 über LEN + alle DATA-Bytes (dest..error)
  uint16_t crc = mi3_crc16(len, data, (uint8_t)(len + 1)); // 9 Bytes
  uint8_t crcLo = (uint8_t)(crc & 0xFF);
  uint8_t crcHi = (uint8_t)((crc >> 8) & 0xFF);

  // Kompletter Frame:
  // [0]=0x55, [1]=0xAA, [2]=LEN, [3..11]=DATA, [12..13]=CRC
  uint8_t frame[2 + 1 + 9 + 2];
  uint8_t f = 0;
  frame[f++] = 0x55;
  frame[f++] = 0xAA;
  frame[f++] = len;
  for (uint8_t i = 0; i < 9; ++i) {
    frame[f++] = data[i];
  }
  frame[f++] = crcLo;
  frame[f++] = crcHi;

  m_bus->write(frame, f);
  m_bus->flush();
}
