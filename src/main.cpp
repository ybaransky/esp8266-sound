#include <Arduino.h>
#include <OneButton.h>

#include "sound_player.h"
#include "web_portal.h"
#include "log.h"

const int BUTTON_PIN = D6;
OneButton button(BUTTON_PIN, true, true);
bool playRequested = false;

void serviceButton() {
  button.tick();
}

void onButtonPress() {
  if (SoundPlayer::isPlaying()) {
    LOG_PRINTLN("button: stop");
    SoundPlayer::stop();
    return;
  }

  LOG_PRINTLN("button: play");
  playRequested = true;
}

void setup() {
  Serial.begin(74880);
  LOG_BLANK_LINE();
  LOG_PRINTF("Firmware built: %s %s", __DATE__, __TIME__);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  button.attachPress(onButtonPress);
  SoundPlayer::setServiceCallback(serviceButton);
  SoundPlayer::begin();
  WebPortal::begin();
}

void loop() {
  WebPortal::tick();
  button.tick();
  if (playRequested) {
    playRequested = false;
    SoundPlayer::playSelected();
  }
  yield();
}
