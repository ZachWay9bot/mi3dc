#include <Arduino.h>
#include <mi3dc.h>

#ifndef RX_PIN
#define RX_PIN 9
#endif

#ifndef TX_PIN
#define TX_PIN 8
#endif

#ifndef DASH_BTN_PIN
#define DASH_BTN_PIN 10
#endif

HardwareSerial& MIBUS = Serial1;

// globaler Light-State, wird per Button getoggelt
static bool g_lightOn = false;

// Drive-Mode, der ans Dash rausgeht (Mapping liegt bei dir)
// z.B. 1 = D, 2 = S, 3 = ECO
static uint8_t g_driveMode = 1;

// Double-Click-Fenster für die Mode-Umschaltung (in ms)
static const uint32_t DOUBLE_CLICK_MS = 350;

void setup() {
  Serial.begin(115200);
  delay(500);

  // Mi3 Bus initialisieren
  Mi3.begin(MIBUS, RX_PIN, TX_PIN, DASH_BTN_PIN, 115200);

  // Byte-Positionen im 0x20-Frame für Gas/Bremse
  Mi3.setControlIndices(4, 5);

  Serial.println(F("Mi3DC_Tx_MiRXStyle + send2dash() + Button: 1x Light, 2x Mode-Cycle"));
}

// Einfacher Helper, der die Fahrmodi zyklisch durchschaltet
static void cycleDriveMode() {
  g_driveMode++;
  if (g_driveMode > 3) g_driveMode = 1;
  Serial.print(F("DriveMode -> "));
  Serial.println(g_driveMode);
}

void loop() {
  // Poll kümmert sich um UART und den Button
  Mi3.poll();

  uint32_t now = millis();

  // --- 1) ESC → Dash: Status alle ~60 ms senden ---
  static uint32_t lastTx = 0;
  if (now - lastTx > 60) {
    lastTx = now;

    static uint8_t demoSpeed = 0;
    demoSpeed = (demoSpeed + 1) % 30;   // 0..29 km/h Demo-Speed

    uint8_t battPct   = 50;
    uint8_t driveMode = g_driveMode;
    bool    lightOn   = g_lightOn;
    uint8_t errorCode = 0;

    send2dash(battPct, demoSpeed, driveMode, lightOn, errorCode);
  }

  // --- 2) Dash-Controls auswerten (inkl. Button) ---
  Mi3DashData d = GetDashData();

  // Click-Logik für 1x/2x kurz drücken
  static bool lastBtn = false;
  static uint8_t clickCount = 0;
  static uint32_t lastClickTime = 0;

  bool btn = d.button;   // true = gerade gedrückt

  // Rising Edge detektieren (Taste von "nicht gedrückt" auf "gedrückt")
  if (btn && !lastBtn) {
    uint32_t nowClick = now;

    if (clickCount == 0 || (nowClick - lastClickTime) > DOUBLE_CLICK_MS) {
      // Neuer Click-Block
      clickCount = 1;
      lastClickTime = nowClick;
    } else {
      // zweiter Klick innerhalb des Fensters → als Double-Click zählen
      clickCount++;
      lastClickTime = nowClick;
    }
  }
  lastBtn = btn;

  // Auswertung, wenn das Double-Click-Fenster abgelaufen ist
  if (clickCount > 0 && (now - lastClickTime) > DOUBLE_CLICK_MS) {
    if (clickCount == 1) {
      // Single-Click: Licht toggeln
      g_lightOn = !g_lightOn;
      Serial.print(F("Light toggle -> "));
      Serial.println(g_lightOn ? F("ON") : F("OFF"));
    } else if (clickCount >= 2) {
      // Double-Click (oder mehr) → Fahrmodus umschalten
      cycleDriveMode();
    }
    clickCount = 0;
  }

  // --- 3) Optional: RX-Status loggen ---
  static uint32_t lastPrint = 0;
  if (now - lastPrint > 0) {
    lastPrint = now;
    Serial.print(F("RX: THR="));
    Serial.print(d.throttle);
    Serial.print(F("  BRK="));
    Serial.print(d.brake);
    Serial.print(F("  BTN="));
    Serial.print(d.button ? F("PRESSED") : F("released"));
    Serial.print(F("  age="));
    Serial.print(now - d.lastUpdateMs);
    Serial.println(F(" ms"));
  }
}
