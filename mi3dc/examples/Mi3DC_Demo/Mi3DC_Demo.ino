#include <Arduino.h>
#include <mi3dc.h>

void setup() {
  Serial.begin(115200);
  delay(500);

  // Mi3 Dash hängt an Serial1, standardmäßig:
  // RX=9, TX=8, Button-Pin=10
  Mi3.begin(Serial1, 9, 8, 10, 115200);

  // Diese beiden Indizes sind die Positionen von Gas/Bremse
  // im Dash→ESC Frame (0x20 0x65). Bei dir: Byte 4 = Gas, Byte 5 = Bremse.
  Mi3.setControlIndices(4, 5);

  Serial.println("Mi3DC Demo start");
}

void loop() {
  // Lib am Leben halten: UART + Button werden hier verarbeitet
  Mi3.poll();

  static uint32_t lastTx = 0;
  uint32_t now = millis();

  // Alle ~120ms irgendwas Hübsches zum Dash schicken
  if (now - lastTx > 60) {
    lastTx = now;

    static uint8_t demoSpeed = 0;
    demoSpeed = (demoSpeed + 1) % 30;  // 0..29 km/h Demo-Speed

    uint8_t battPct   = 80;   // 80% Akku
    uint8_t driveMode = 2;    // z.B. "S"
    bool    lightOn   = false;
    uint8_t errorCode = 0;    // kein Fehler

    // Das reicht, um das Dash am Leben zu halten
    send2dash(battPct, demoSpeed, driveMode, lightOn, errorCode);
  }

  // Input vom Dash auslesen
  Mi3DashData d = GetDashData();

  static uint32_t lastPrint = 0;
  if (now - lastPrint > 500) {
    lastPrint = now;
    Serial.print("RX: THR=");
    Serial.print(d.throttle);
    Serial.print("  BRK=");
    Serial.print(d.brake);
    Serial.print("  BTN=");
    Serial.print(d.button ? "PRESSED" : "released");
    Serial.print("  age=");
    Serial.print(now - d.lastUpdateMs);
    Serial.println(" ms");
  }
}
