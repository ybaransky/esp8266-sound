#pragma once

#include <Arduino.h>

// ===========================================================================
// Hardware pin assignments and wiring  (Wemos D1 mini / ESP8266)
// ===========================================================================
//
// Single source of truth for pin usage. See WIRING.md for the full
// explanation, parts list, and why the buzzer needs an NPN buffer on D8.
//
//   | Function   | Pin | GPIO | Notes                                        |
//   | ---------- | --- | ---- | -------------------------------------------- |
//   | Buzzer     | D8  | 15   | Boot-strap pin, must be LOW at reset. Driven |
//   |            |     |      | through an inverting NPN (see below).        |
//   | Button     | D6  | 12   | Momentary switch to GND, internal pull-up.   |
//   | Status LED | D4  | 2    | On-board LED, active LOW (LED_BUILTIN).       |
//
// ---------------------------------------------------------------------------
// Buzzer wiring  (active-LOW KY-006 module via an inverting NPN)
// ---------------------------------------------------------------------------
//
//                      3.3V ---- Module VCC
//                       GND ---- Module GND
//                                 Module S
//                                     |
//                                     |
//   D8(GPIO15) --[1k]--+-------- B    C   NPN (2N2222 / BC547 / S8050)
//                      |              E
//                    [10k]            |
//                      |              |
//                     GND            GND
//
//   D8 HIGH -> NPN on  -> module S pulled LOW -> buzzer sounds.
//   D8 LOW  -> NPN off -> module idle.
//
//   The 10k holds GPIO15 LOW at reset so the board boots. The module cannot
//   sit on D8 directly: it idles its S pin at 2.7V, which violates the strap.
//
// ---------------------------------------------------------------------------
// Button wiring
// ---------------------------------------------------------------------------
//
//   D6 ----[ button ]---- GND
//
// ---------------------------------------------------------------------------

const uint8_t BUZZER_PIN = D8;               // GPIO15
const uint8_t BUTTON_PIN = D6;               // GPIO12
const uint8_t STATUS_LED_PIN = LED_BUILTIN;  // GPIO2 / D4, active LOW
