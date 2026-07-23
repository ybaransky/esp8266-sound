# D8 Buzzer Wiring

The buzzer module is active LOW, so it cannot connect to D8 directly.
D8 drives an NPN transistor, and that transistor pulls the module's S pin low.

## Two Transistors, Do Not Confuse Them

- The **PNP** is already inside the buzzer module. You do not wire it or buy it.
  It is the reason the module is active LOW.
- The **NPN** is the part you add, between D8 and the module's S pin. It inverts
  D8 so that a HIGH on D8 pulls the module's S pin low.

## Pin Map

| Function | Pin | GPIO |
| -------- | --- | ---- |
| Buzzer   | D8  | 15   |
| Button   | D6  | 12   |
| LED      | D4  | 2    |
| Serial   | TX/RX | 1/3 |

## Exact Wiring

```text
Wemos D1 mini / ESP8266

Module VCC ------------------- 3.3V
Module GND ------------------- GND
Module S   ------------------- collector of NPN

D8 (GPIO15) ---- 1k resistor ---- base of NPN transistor
                               |
                            10k resistor
                               |
                              GND

emitter of NPN --------------- GND
ESP8266 GND ------------------ same GND
```

## Pin-To-Pin Version

- D8 -> 1k resistor -> transistor B (base)
- transistor B -> 10k resistor -> GND
- transistor E (emitter) -> GND
- transistor C (collector) -> module S
- module VCC -> 3.3V
- module GND -> GND
- D1 mini GND -> same ground rail

## ASCII Schematic

```text
                     3.3V ---- Module VCC
                      GND ---- Module GND
                                Module S
                                    |
                                    |
D8(GPIO15) --[1k]--+--------- B     C   NPN transistor
                   |               ...
                 [10k]              E
                   |                |
                  GND              GND
```

## Why The Module Is Active LOW

This section is about the PNP inside the module, not the NPN you add.

The module holds its S pin at 2.7V when nothing is attached to it.
That is 3.3V minus one diode drop, which is the signature of a PNP transistor:

- emitter to VCC
- base one V_BE below VCC, wired to S through the module's resistor
- collector to the buzzer

Pulling S low turns the PNP on and drives the buzzer.

A multimeter in resistance mode reads megohms from S to VCC, because the ohms
ranges cannot forward bias the base-emitter junction. Use diode mode instead and
the junction shows up as roughly 0.6V to 0.7V in one direction.

## Why This Circuit Works

- D8 is GPIO15, a boot-strap pin. It must be LOW at reset or the chip boots to
  the wrong mode and never runs.
- Connecting the module's S pin to D8 directly holds GPIO15 at 2.7V through the
  forward biased base-emitter junction, so the board does not boot.
- A pulldown on D8 does not fix it. Beating the module's ~1k source impedance
  needs a resistor around 330 ohms, which lands near the 0.8V logic threshold,
  wastes 8mA continuously, and fights the pin whenever it drives high.
- With the NPN buffer, D8 connects only to a 1k base resistor and a 10k pulldown.
  There is no path from D8 to VCC, so the strap requirement is satisfied.
- At reset D8 is low, the NPN is off, and the module's S pin floats back to 2.7V
  on its own. A passive piezo makes no sound at a steady DC level, so it stays
  silent.

## Logic Is Inverted, And That Is Fine

D8 HIGH turns the NPN on, pulls S low, and sounds the buzzer.

`tone()` produces a 50% duty square wave, so inverting it gives the same waveform
at the same frequency. It sounds identical.

## Current

- base drive: (3.3V - 0.7V) / 1k = about 2.6mA
- pulling S low: about 2.4mA through the module's own base resistor

## Parts

- 1k base resistor
- 10k base pulldown resistor
- 2N2222, PN2222, BC547, or S8050 NPN transistor
  (nothing to buy for the PNP, it is already on the module)

A 2N7000 MOSFET works too: gate to D8, drain to module S, source to GND,
10k from gate to GND, and no 1k needed.

## Alternative Without A Transistor

D7 (GPIO13) has no boot-strap role, so the module's S pin can connect to it
directly with no support parts. D5, D6, D1, and D2 work the same way.

Avoid D3 (GPIO0) and D4 (GPIO2), which must be HIGH at reset, and D0 (GPIO16),
where `tone()` does not work.
