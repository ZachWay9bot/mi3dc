# mi3dc – Mi Electric Scooter 3 Dashboard Helper

Kleine Arduino-Lib, um das **Mi Electric Scooter 3 Dashboard (Mi3 Dash)** wie ein ganz normales
„dummes Terminal“ zu benutzen:

- Du schickst dem Dash einen Status-Frame: `Speed`, `Battery`, `Drive Mode`, `Light`, `Error`
- Du liest vom Dash: `Throttle`, `Brake` und den **Dash-Button**

Gedacht für ESP32-Projekte (Bridges, VESC-Adapter, eigene ESC-Simulatoren, Dash-Tester etc.).

---

## TL;DR – Was kann das Ding?

- Protokoll: 0x55 0xAA Mi3 / M365‑Style Frames
- **RX**: Dash → ESP32
  - Liest die 0x20 0x64 / 0x20 0x65 Frames vom Mi3 Dash
  - Du bekommst: `throttle`, `brake`, `button`, `lastUpdateMs`
- **TX**: ESP32 → Dash
  - Baut einen 0x21 0x64 Statusframe:
    - Akku‑Prozent
    - Integer‑Speed in km/h
    - Fahrmodus (1/2/3 – Mapping ist dir überlassen)
    - Licht (Night/Headlight on/off)
    - Error‑Byte (frei nutzbar)
- Extra Example:
  - 1x kurz Button → Licht toggeln
  - 2x kurz Button → Fahrmodus durchschalten

---

## Hardware / Pins

Default-Pins im Example (ESP32):

- `Serial1` hängt am Mi3 Dash
  - `RX_PIN = 9`  → Mi3 TX
  - `TX_PIN = 8`  → Mi3 RX
- `DASH_BTN_PIN = 10`
  - Dash-Button-Leitung (aktiv-low, also Pullup auf ESP-Seite)

Das kannst du über Defines oder über `Mi3.begin()` anpassen.

---

## Installation

1. ZIP herunterladen
2. In Arduino IDE: **Sketch → Bibliothek einbinden → .ZIP-Bibliothek hinzufügen…**
3. In deinem Sketch:

```cpp
#include <mi3dc.h>
```

---

## API Überblick

### `Mi3.begin(...)`

```cpp
Mi3.begin(Serial1, 9, 8, 10, 115200);
```

- `bus`    – HardwareSerial (z.B. `Serial1`)
- `rxPin`  – ESP32 RX-Pin (Mi3 TX)
- `txPin`  – ESP32 TX-Pin (Mi3 RX)
- `btnPin` – Pin, an dem der Dash-Button hängt (aktiv-low)
- `baud`   – Baudrate (Mi3: 115200)

### `Mi3.setControlIndices(throttleIdx, brakeIdx)`

Sagt der Lib, an welchen Byte-Positionen (im 0x20‑Frame) Gas und Bremse stehen.

Beispiel (bewährt für Mi3):

```cpp
Mi3.setControlIndices(4, 5);  // data[4] = Gas, data[5] = Bremse
```

### `Mi3.poll()`

Muss im `loop()` laufen. Liest UART, parst Frames und aktualisiert `Mi3DashData`:

```cpp
void loop() {
  Mi3.poll();
  ...
}
```

### `Mi3DashData GetDashData()` / `Mi3.getDashData()`

Gibt dir die letzten bekannten Werte vom Dash:

```cpp
Mi3DashData d = GetDashData();

// d.throttle     -> Gas-Rohwert 0..255
// d.brake        -> Brems-Rohwert 0..255
// d.button       -> true = Button gedrückt
// d.lastUpdateMs -> millis() des letzten gültigen Frames
```

### `send2dash(...)` / `Mi3.send2dash(...)`

Baut einen ESC→Dash Statusframe und schickt ihn raus:

```cpp
send2dash(battPct, speedKmh, driveMode, lightOn, errorCode);
```

- `battPct`   – Akku in % (0..100, 1:1 ins Frame)
- `speedKmh`  – Integer‑Speed in km/h (Anzeige im Dash)
- `driveMode` – dein Fahrmodus (1/2/3 etc.)
- `lightOn`   – `true` → Night/Headlight an (Byte `N = 0x64`)
- `errorCode` – komplettes Error-Byte (letztes Payload-Byte, 0..255)

Frame-Struktur intern:

```text
55 AA  L  21 64 00  drive  batt  N  s1  speed  error  ck0  ck1
```

- `drive` = `driveMode`
- `batt`  = `battPct`
- `N`     = `lightOn ? 0x64 : 0x00`
- `speed` = `speedKmh`
- `error` = `errorCode`

---

## Beispiele

### `Mi3DC_Demo`

Minimalbeispiel:

- Liest Gas/Bremse/Button und loggt sie auf Serial.
- Schickt einen Dummy‑Status (Akku 80 %, Mode 2, Licht aus) zum Dash.

Gut zum Verifizieren, ob die Verkabelung und das Protokoll an sich stimmen.

### `Mi3DC_Tx_MiRXStyle`

„Vollgas‑Example“ nach deinem Setup:

- **RX:**
  - `Mi3.setControlIndices(4, 5);`
  - `GetDashData()` liefert Throttle, Brake, Button.
- **TX:**
  - Alle 60 ms → `send2dash(...)` mit:
    - `battPct = 50`
    - `driveMode = g_driveMode` (startet bei 1)
    - `lightOn = g_lightOn` (wird über Button gesteuert)
    - `errorCode = 0`
- **Button‑Logik:**
  - 1x kurz drücken → `g_lightOn` toggelt → Dash-Lichtsymbol an/aus
  - 2x kurz drücken (innerhalb ~350 ms) → `g_driveMode` wird zyklisch gewechselt

Das ist der perfekte Einstieg, um das später an VESC‑Daten anzubinden.

---

## Typische Verwendung mit VESC

High-Level Idee (Pseudocode):

```cpp
void loop() {
  Mi3.poll();          // Dash lesen
  getVescData();       // VESC Telemetrie holen

  Mi3DashData d = GetDashData();

  // d.throttle / d.brake → in VESC Current / Brake mapppen
  // ...

  // VESC-Status zurück ins Dash
  send2dash(
    batteryPercentFromVesc,
    speedFromVescKmh,
    currentDriveMode,
    lightOnState,
    errorByte
  );
}
```

Die Lib kümmert sich nur um das Mi3‑Protokoll, nicht um die VESC‑Details.
So bleibt das alles schön modular.

---

## Lizenz

MIT. Mach damit, was du willst – aber bitte nichts Dummes im Straßenverkehr. 😉

Siehe [`LICENSE`](LICENSE).
