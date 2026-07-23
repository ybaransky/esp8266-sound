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
bool namesAsJson(String &json);

}  // namespace SoundPlayer
