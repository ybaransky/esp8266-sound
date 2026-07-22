# D8 Buzzer Wiring

Use D8 only to drive a transistor, not the buzzer directly.

## Exact Wiring

For a passive buzzer with an NPN transistor such as 2N2222, PN2222, or BC547:

```text
Wemos D1 mini / ESP8266

D8 (GPIO15) ---- 1k resistor ---- base of NPN transistor
                               |
                            10k resistor
                               |
                              GND

3.3V ------------------------- buzzer +
buzzer - --------------------- collector of NPN
emitter of NPN --------------- GND

ESP8266 GND ------------------ same GND
```

## Pin-To-Pin Version

- D8 -> 1k resistor -> transistor B (base)
- transistor B -> 10k resistor -> GND
- transistor E (emitter) -> GND
- transistor C (collector) -> buzzer -
- buzzer + -> 3.3V
- D1 mini GND -> same ground rail

## Why This Works

- At boot, D8 stays LOW, which is what ESP8266 requires.
- LOW on D8 keeps the transistor OFF.
- The buzzer is isolated from the boot strap pin, so upload and boot still work.

## ASCII Schematic

```text
                +3.3V
                  |
                  |
               [ BUZZER ]
                  |
                  +---------- C
                             |
D8(GPIO15) --[1k]--+-------- B   NPN transistor
                   |         E
                 [10k]       |
                   |         |
                  GND       GND
```

## Optional Protection Diode

If your buzzer is a magnetic buzzer, add a diode across the buzzer:
- diode cathode to buzzer +
- diode anode to buzzer -

```text
           +3.3V
             |
          buzzer +
             |
          buzzer - ---- collector
             |            |
             +---|<|------+
```

That diode is usually not needed for a pure piezo disc, but it is safe for magnetic buzzer modules.

## Parts

- 1k base resistor
- 10k base pulldown resistor
- 2N2222 or similar NPN transistor
- optional 1N4148 or 1N400x diode for magnetic buzzers

## Notes

- D8 is GPIO15, a boot-strap pin on the ESP8266.
- It must remain LOW during boot.
- Do not connect the buzzer directly to D8 if you want reliable boot and upload behavior.
