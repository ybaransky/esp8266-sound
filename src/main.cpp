#include <Arduino.h>
#include <OneButton.h>

#include "build_time.h"
#include "hardware.h"
#include "sound_player.h"
#include "web_portal.h"
#include "log.h"

OneButton button(BUTTON_PIN, true, true);

// Runs repeatedly while a sound is playing (playback blocks the main loop).
// It services both the button and the web server so a physical or web Stop
// can be received mid-playback. Safe from reentrancy because playback is
// always started from loop() via SoundPlayer::service(), never from inside an
// HTTP handler.
void serviceDuringPlayback() {
  button.tick();
  WebPortal::tick();
}

void onButtonPress() {
  if (SoundPlayer::isPlaying()) {
    LOG_PRINTLN("button: stop");
    SoundPlayer::stop();
    return;
  }

  LOG_PRINTLN("button: play");
  SoundPlayer::requestPlaySelected();
}

void setup() {
  Serial.begin(74880);
  LOG_BLANK_LINE();
  LOG_PRINTF("Firmware built: %s", BUILD_TIME);
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, HIGH);

  button.attachPress(onButtonPress);
  SoundPlayer::setServiceCallback(serviceDuringPlayback);
  SoundPlayer::begin();
  WebPortal::begin();
}

void loop() {
  WebPortal::tick();
  button.tick();
  SoundPlayer::service();
  yield();
}
