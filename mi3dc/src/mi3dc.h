#pragma once
#ifndef MI3DC_H
#define MI3DC_H

#include <Arduino.h>

// Kleiner Helper für das Mi Electric Scooter 3 Dashboard (Mi3 Dash)
// Idee: Dash wie ein "dummes Terminal" behandeln:
// - wir schicken Status (Speed, Batterie, Mode, Licht, Error) hin
// - wir lesen Throttle / Brake + Button zurück

// Das hier sind die aufbereiteten Dash-Daten, die du bequem abfragen kannst.
struct Mi3DashData {
  uint8_t  throttle;      // Gas-Rohwert 0..255 (kommt direkt aus dem 0x20-Frame)
  uint8_t  brake;         // Brems-Rohwert 0..255
  bool     button;        // Dash-Button an GPIO10 (aktiv-low → hier schon "true = gedrückt")
  uint32_t lastUpdateMs;  // millis() beim letzten gültigen RX-Frame vom Dash
};

// Hauptklasse für das Mi3-Dashboard
class Mi3DC {
public:
  Mi3DC();

  // UART + Button initialisieren
  //
  // bus    = HardwareSerial, i.d.R. Serial1
  // rxPin  = RX vom Mi3 Dash
  // txPin  = TX zum Mi3 Dash
  // btnPin = GPIO vom Dash-Button (geht auf ESP32, default 10)
  // baud   = Mi3 Bus-Baudrate (115200)
  void begin(HardwareSerial &bus = Serial1,
             int rxPin = 9,
             int txPin = 8,
             int btnPin = 10,
             uint32_t baud = 115200);

  // Sag der Lib, an welchen Byte-Positionen im Dash→ESC-Frame
  // Gas und Bremse stehen (0‑basiert).
  //
  // Beispiel: setControlIndices(4, 5);
  // → data[4] = Throttle, data[5] = Brake
  void setControlIndices(uint8_t throttleIdx, uint8_t brakeIdx);

  // ESC → Dash Status schicken
  //
  // battPercent = Akku in % (0..100, wird 1:1 ins Frame geschrieben)
  // speedKmh    = Speed als Integer km/h (Anzeige im Dash)
  // driveMode   = Fahrmodus (1=D, 2=S, 3=ECO o.ä. → rein dein Mapping)
  // lightOn     = true → Night/Headlight an (entspricht "N" = 0x64)
  // errorCode   = Fehlerbyte (komplettes letztes Payload-Byte, frei nutzbar)
  //
  // Intern wird das als 0x55 0xAA ... 0x21 0x64 Statusframe gebaut.
  void send2dash(uint8_t battPercent,
                 uint8_t speedKmh,
                 uint8_t driveMode,
                 bool    lightOn,
                 uint8_t errorCode);

  // Muss im loop() laufen, damit
  // - UART vom Dash geparsed wird
  // - Button gelesen wird
  void poll();

  // Letzte bekannten Dash-Daten (Gas, Bremse, Button)
  Mi3DashData getDashData() const;

private:
  HardwareSerial *m_bus;
  int   m_rxPin;
  int   m_txPin;
  int   m_btnPin;
  bool  m_btnActiveLow;
  bool  m_started;

  // Indizes im empfangenen DATA[] Frame für Gas/Bremse
  int8_t m_throttleIdx;
  int8_t m_brakeIdx;

  static const uint8_t MAX_FRAME = 64;

  uint8_t m_len;                  // LEN-Byte aus dem 0x55 0xAA Protokoll
  uint8_t m_buf[MAX_FRAME];       // Datenbytes nach LEN (dest/cmd/sub/...)
  uint8_t m_idx;                  // Füllstand RX-Puffer
  uint8_t m_crcLo;                // Low-Byte der CRC beim RX

  enum RxState : uint8_t {
    WAIT_HEADER1,
    WAIT_HEADER2,
    READ_LEN,
    READ_DATA,
    READ_CK1,
    READ_CK2
  } m_state;

  Mi3DashData m_last;

  void resetParser();
  void handleByte(uint8_t b);
  void handleFrame(const uint8_t *data, uint8_t len);
};

// Globales Objekt wie bei Wire, Serial etc.
// → so kannst du einfach "Mi3.begin(...)" benutzen.
extern Mi3DC Mi3;

// C-Wrapper im "Arduino-Stil", falls du es lieber so magst:
inline void send2dash(uint8_t battPercent,
                      uint8_t speedKmh,
                      uint8_t driveMode,
                      bool    lightOn,
                      uint8_t errorCode)
{
  Mi3.send2dash(battPercent, speedKmh, driveMode, lightOn, errorCode);
}

inline Mi3DashData GetDashData()
{
  return Mi3.getDashData();
}

#endif // MI3DC_H
