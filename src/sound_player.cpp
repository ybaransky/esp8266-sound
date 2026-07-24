#include "sound_player.h"

#include <ArduinoJson.h>
#include <LittleFS.h>

#include "hardware.h"
#include "log.h"

// ---------------------------------------------------------------------------
// Software volume control
//
// When enabled, notes are produced with a PWM square wave whose duty cycle
// sets the loudness, instead of tone(). A square wave is loudest at 50% duty;
// lowering the duty makes it quieter at the same pitch.
//
// To DISABLE this and control volume in hardware instead (option 2: lower the
// module's supply voltage, see WIRING.md), set USE_SOFTWARE_VOLUME to 0. That
// reverts the player to plain full-volume tone(), so a hardware divider is the
// only thing affecting loudness.
#define USE_SOFTWARE_VOLUME 1

namespace {

const int PWM_RANGE = 255;
const int MAX_DUTY = PWM_RANGE / 2;  // 50% duty cycle = loudest
int volumePercent = 40;              // 0..100, settable at runtime from the web UI
bool toneSounding = false;           // true while a note is actively driven

int currentDuty() {
  return (constrain(volumePercent, 0, 100) * MAX_DUTY) / 100;
}
const char SOUND_FILE[] = "/sounds.json";
const char DEFAULT_SOUNDS[] PROGMEM =
    "{\"sounds\":["
    "{\"name\":\"Welcome\",\"notes\":[262,330,392,523],\"durations\":[8,8,8,4]},"
    "{\"name\":\"Doorbell\",\"notes\":[659,523],\"durations\":[4,2]},"
    "{\"name\":\"Alert\",\"notes\":[880,0,880,0,880],"
    "\"durations\":[8,16,8,16,8]},"
    "{\"name\":\"Cute Connection\",\"notes\":[659,1319,1760],"
    "\"durations\":[20,18,16]},"
    "{\"name\":\"Cute Disconnection\",\"notes\":[659,1760,1319],"
    "\"durations\":[20,18,20]},"
    "{\"name\":\"Cute Mode 3\",\"notes\":[1319,1568,2349],"
    "\"durations\":[20,20,3]}"
    "]}";
bool filesystemMounted = false;
String selectedSoundName;
String pendingSoundName;
bool playPending = false;
bool playing = false;
bool stopRequested = false;
SoundPlayer::ServiceCallback serviceCallback = nullptr;

bool cooperativeDelay(unsigned long duration) {
  unsigned long started = millis();
  while (millis() - started < duration) {
    if (serviceCallback != nullptr) {
      serviceCallback();
    }
    if (stopRequested) {
      return false;
    }
    delay(1);
    yield();
  }
  return true;
}

bool createDefaultSoundsIfMissing() {
  if (LittleFS.exists(SOUND_FILE)) {
    return true;
  }

  LOG_PRINTLN("/sounds.json is missing; creating the default sound catalog");
  File file = LittleFS.open(SOUND_FILE, "w");
  if (!file) {
    LOG_PRINTLN("Could not create /sounds.json");
    return false;
  }

  size_t written = file.print(FPSTR(DEFAULT_SOUNDS));
  file.close();
  if (written == 0) {
    LOG_PRINTLN("Could not write the default sound catalog");
    return false;
  }

  LOG_PRINTF("Created /sounds.json (%u bytes)", static_cast<unsigned>(written));
  return true;
}

bool ensureSoundsReady() {
  if (!filesystemMounted) {
    filesystemMounted = LittleFS.begin();
    if (!filesystemMounted) {
      LOG_PRINTLN("Cannot access sounds: LittleFS is not mounted");
      return false;
    }
    LOG_PRINTLN("LittleFS mounted");
  }
  return createDefaultSoundsIfMissing();
}

bool printFilesystemFileList() {
  LOG_PRINTLN("LittleFS files:");
  Dir directory = LittleFS.openDir("/");
  bool foundFile = false;
  while (directory.next()) {
    foundFile = true;
    String name = directory.fileName();
    LOG_PRINTF("  %s (%u bytes)", name.c_str(),
               static_cast<unsigned>(directory.fileSize()));
  }
  if (!foundFile) {
    LOG_PRINTLN("  (empty)");
  }
  return foundFile;
}

bool loadSoundDocument(JsonDocument &document) {
  if (!ensureSoundsReady()) {
    return false;
  }

  File file = LittleFS.open(SOUND_FILE, "r");
  if (!file) {
    LOG_PRINTLN("Could not open /sounds.json after verifying it exists");
    return false;
  }

  DeserializationError error = deserializeJson(document, file);
  file.close();
  if (error) {
    LOG_PRINTF("Invalid /sounds.json: %s", error.c_str());
    return false;
  }
  return true;
}

void startTone(int frequency) {
  toneSounding = true;
#if USE_SOFTWARE_VOLUME
  analogWriteFreq(frequency);
  analogWrite(BUZZER_PIN, currentDuty());
#else
  tone(BUZZER_PIN, frequency);
#endif
}

void stopTone() {
  toneSounding = false;
#if USE_SOFTWARE_VOLUME
  analogWrite(BUZZER_PIN, 0);
  digitalWrite(BUZZER_PIN, LOW);  // hold LOW so the buzzer is silent and D8 stays strap-safe
#else
  noTone(BUZZER_PIN);
#endif
}

void play(JsonObjectConst sound) {
  JsonArrayConst notes = sound["notes"].as<JsonArrayConst>();
  JsonArrayConst durations = sound["durations"].as<JsonArrayConst>();
  size_t count = min(notes.size(), durations.size());
  int tempo = sound["tempo"] | 0;
  int wholeNote = tempo > 0 ? (60000 * 4) / tempo : 0;
  playing = true;
  stopRequested = false;

  for (size_t index = 0; index < count; ++index) {
    int frequency = notes[index] | 0;
    int divisor = durations[index] | 4;
    int noteDuration;
    if (tempo > 0) {
      if (divisor == 0) {
        divisor = 4;
      }
      noteDuration = wholeNote / abs(divisor);
      if (divisor < 0) {
        noteDuration = noteDuration * 3 / 2;
      }
    } else {
      if (divisor <= 0) {
        divisor = 4;
      }
      noteDuration = 1000 / divisor;
    }

    // A note sounds for most of its slot, then a short gap articulates it from
    // the next. Unlike tone(duration), PWM runs until stopped, so we time the
    // sound and the gap explicitly here (this also matches the old tone timing).
    int soundDuration = tempo > 0 ? noteDuration * 9 / 10 : noteDuration;
    int gapDuration = tempo > 0 ? noteDuration - soundDuration : noteDuration * 0.30;

    if (frequency > 0) {
      startTone(frequency);
    }
    digitalWrite(STATUS_LED_PIN, LOW);
    if (!cooperativeDelay(soundDuration)) {
      break;
    }
    digitalWrite(STATUS_LED_PIN, HIGH);
    stopTone();
    if (gapDuration > 0) {
      if (!cooperativeDelay(gapDuration)) {
        break;
      }
    }
  }

  stopTone();
  digitalWrite(STATUS_LED_PIN, HIGH);
  playing = false;
  stopRequested = false;
}

}  // namespace

namespace SoundPlayer {

bool begin() {
#if USE_SOFTWARE_VOLUME
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  analogWriteRange(PWM_RANGE);
#endif

  filesystemMounted = LittleFS.begin();
  if (!filesystemMounted) {
    LOG_PRINTLN("LittleFS mount failed. Upload the filesystem image.");
    return false;
  }

  LOG_PRINTLN("LittleFS mounted");
  bool soundsReady = ensureSoundsReady();
  printFilesystemFileList();
  if (soundsReady) {
    JsonDocument document;
    if (loadSoundDocument(document)) {
      selectedSoundName = document["sounds"][0]["name"].as<String>();
      if (!selectedSoundName.isEmpty()) {
        LOG_PRINTF("Selected sound: %s", selectedSoundName.c_str());
      }
    }
  }
  return soundsReady;
}

void setServiceCallback(ServiceCallback callback) {
  serviceCallback = callback;
}

bool isPlaying() {
  // A queued-but-not-yet-started sound counts as playing so callers (and the
  // web status poll) never see a false idle in the gap before service() runs.
  return playing || playPending;
}

void stop() {
  playPending = false;
  if (playing) {
    stopRequested = true;
    stopTone();
    digitalWrite(STATUS_LED_PIN, HIGH);
    LOG_PRINTLN("Sound stopped");
  }
}

bool select(const String &name) {
  JsonDocument document;
  if (!loadSoundDocument(document)) {
    return false;
  }

  for (JsonObjectConst sound : document["sounds"].as<JsonArrayConst>()) {
    if (name == sound["name"].as<const char *>()) {
      selectedSoundName = name;
      LOG_PRINTF("Selected sound: %s", selectedSoundName.c_str());
      return true;
    }
  }
  return false;
}

bool playSelected() {
  if (selectedSoundName.isEmpty()) {
    LOG_PRINTLN("No sound is selected");
    return false;
  }
  return playByName(selectedSoundName);
}

bool requestPlay(const String &name) {
  JsonDocument document;
  if (!loadSoundDocument(document)) {
    return false;
  }
  for (JsonObjectConst sound : document["sounds"].as<JsonArrayConst>()) {
    if (name == sound["name"].as<const char *>()) {
      pendingSoundName = name;
      playPending = true;
      return true;
    }
  }
  return false;
}

void requestPlaySelected() {
  if (selectedSoundName.isEmpty()) {
    LOG_PRINTLN("No sound is selected");
    return;
  }
  pendingSoundName = selectedSoundName;
  playPending = true;
}

void service() {
  if (playPending && !playing) {
    playPending = false;
    playByName(pendingSoundName);
  }
}

void setVolume(int percent) {
  volumePercent = constrain(percent, 0, 100);
#if USE_SOFTWARE_VOLUME
  // If a note is sounding right now, re-apply the duty so the change is heard
  // immediately instead of only on the next note.
  if (toneSounding) {
    analogWrite(BUZZER_PIN, currentDuty());
  }
#endif
}

int getVolume() {
  return volumePercent;
}

bool playByName(const String &name) {
  JsonDocument document;
  if (!loadSoundDocument(document)) {
    return false;
  }

  for (JsonObjectConst sound : document["sounds"].as<JsonArrayConst>()) {
    if (name == sound["name"].as<const char *>()) {
      const char *path = sound["file"];
      if (path != nullptr) {
        File file = LittleFS.open(path, "r");
        if (!file) {
          LOG_PRINTF("Could not open song file: %s", path);
          return false;
        }
        JsonDocument songDocument;
        DeserializationError error = deserializeJson(songDocument, file);
        file.close();
        if (error) {
          LOG_PRINTF("Invalid song file %s: %s", path, error.c_str());
          return false;
        }
        play(songDocument.as<JsonObjectConst>());
        return true;
      }
      play(sound);
      return true;
    }
  }
  return false;
}

bool namesAsJson(String &json) {
  JsonDocument source;
  if (!loadSoundDocument(source)) {
    return false;
  }

  JsonDocument response;
  JsonArray output = response["sounds"].to<JsonArray>();
  for (JsonObjectConst sound : source["sounds"].as<JsonArrayConst>()) {
    JsonObject item = output.add<JsonObject>();
    item["name"] = sound["name"];
  }

  json = "";
  serializeJson(response, json);
  return true;
}

}  // namespace SoundPlayer
