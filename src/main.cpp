#include <Arduino.h>
#include <OneButton.h>
#include "pitches.h"
// GPIO15 must be LOW at reset. The buzzer module is active LOW (PNP), so it cannot sit on D8
// directly. D8 drives an NPN that pulls the module's S pin down instead. See WIRING.md.
const int BUZZZER_PIN = D8;
const int BUTTON_PIN = D6;  // GPIO12, button to GND, no external resistor needed

OneButton button(BUTTON_PIN, true, true); // active LOW, internal pull-up

// notes in the melody:
int melody[] = {
  NOTE_C4, NOTE_G3, NOTE_G3, NOTE_A3, NOTE_G3, 0, NOTE_B3, NOTE_C4
};

// note durations: 4 = quarter note, 8 = eighth note, etc.:
int noteDurations[] = {
  4, 8, 8, 4, 4, 4, 4, 4
};

const int NOTE_COUNT = sizeof(melody) / sizeof(melody[0]);

void playMelody() {
  // iterate over the notes of the melody:
  for (int thisNote = 0; thisNote < NOTE_COUNT; thisNote++) {

    // to calculate the note duration, take one second divided by the note type.
    //e.g. quarter note = 1000 / 4, eighth note = 1000/8, etc.
    int noteDuration = 1000 / noteDurations[thisNote];
    tone(BUZZZER_PIN, melody[thisNote], noteDuration);

    // blink the on-board LED in step with the note (it is active LOW):
    digitalWrite(LED_BUILTIN, LOW);
    delay(noteDuration);
    digitalWrite(LED_BUILTIN, HIGH);

    // to distinguish the notes, set a minimum time between them.
    // The note's duration + 30% seems to work well:
    int pauseBetweenNotes = noteDuration * 1.30;
    delay(pauseBetweenNotes - noteDuration);
    // stop the tone playing:
    noTone(BUZZZER_PIN);
  }
}

void onClick() {
  Serial.println("button: single press");
  playMelody();
}

void setup() {
  Serial.begin(74880); // matches monitor_speed, so the ROM boot log is readable too

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH); // active LOW, so HIGH is off

  button.attachClick(onClick);

  playMelody();
}

void loop() {
  button.tick();

  static unsigned long lastPrint = 0;
  unsigned long now = millis();
  if (now - lastPrint >= 10000) {
    lastPrint = now;
    Serial.print("millis: ");
    Serial.println(now);
  }
}

