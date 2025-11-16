# mi3dc – Mi Electric Scooter 3 Dashboard Helper

Small Arduino helper library to talk to the **Mi Electric Scooter 3 dashboard (Mi3 Dash)** like a dumb terminal:

- You **send status** to the dash: `speed`, `battery`, `drive mode`, `light`, `error`.
- You **read controls** from the dash: `throttle`, `brake`, `button`.

Designed mainly for ESP32 projects (bridges, VESC adapters, ESC simulators, dashboard testers, etc.).

---

## Features

- Protocol: 0x55 0xAA Mi3 / M365-style frames.
- **RX (Dash → ESP32)**  
  - Parses 0x20 0x64 / 0x20 0x65 frames from the Mi3 dash.  
  - Gives you: `throttle`, `brake`, `button`, `lastUpdateMs`.
- **TX (ESP32 → Dash)**  
  - Builds a 0x21 0x64 **status frame**:
    - Battery percentage
    - Integer speed in km/h
    - Drive mode (1/2/3 – you decide the mapping)
    - Light on/off (Night/Headlight field)
    - Error byte (full last payload byte)
- Example with button logic:
  - **Single click** → toggle light.
  - **Double click** → cycle drive mode (1 → 2 → 3 → 1 …).

---

## Hardware / Pins

Default wiring in the examples (ESP32, single-wire UART on the Mi3 yellow line):

- `Serial1` connected to the Mi3 dashboard bus:
  - `RX_PIN = 9`  → 560 Ω → dash yellow wire  
  - `TX_PIN = 8`  → 560 Ω → dash yellow wire  
  *(two separate 560 Ω resistors, both going to the same yellow bus line)*

- `DASH_BTN_PIN = 10`  
  - GPIO where the dash button signal is connected (active-low).  
  - The library already converts that to a simple `bool` (`true = pressed`).

You can change the pins via `#define` or directly in `Mi3.begin()`.

---

## Installation

1. Download the ZIP of this repo (or the library package).
2. In Arduino IDE:  
   **Sketch → Include Library → Add .ZIP Library…**  
   and select the ZIP.
3. In your sketch:

```cpp
#include <mi3dc.h>
