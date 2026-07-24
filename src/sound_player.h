#pragma once

#include <Arduino.h>

namespace SoundPlayer {

using ServiceCallback = void (*)();

bool begin();
void setServiceCallback(ServiceCallback callback);
bool isPlaying();
void stop();
bool select(const String &name);
bool playSelected();
bool playByName(const String &name);
bool requestPlay(const String &name);   // queue a named sound; false if it does not exist
void requestPlaySelected();             // queue the currently selected sound
void service();                         // start queued playback when idle; call from loop()
void setVolume(int percent);            // 0..100; takes effect immediately, even mid-note
int getVolume();                        // current volume, 0..100
bool namesAsJson(String &json);

}  // namespace SoundPlayer
