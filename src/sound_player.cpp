#include "sound_player.h"

#include <ArduinoJson.h>
#include <LittleFS.h>

#include "log.h"

namespace {

const int BUZZER_PIN = D8;
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

    if (frequency > 0) {
      int toneDuration = tempo > 0 ? noteDuration * 9 / 10 : noteDuration;
      tone(BUZZER_PIN, frequency, toneDuration);
    }

    digitalWrite(LED_BUILTIN, LOW);
    if (!cooperativeDelay(noteDuration)) {
      break;
    }
    digitalWrite(LED_BUILTIN, HIGH);
    noTone(BUZZER_PIN);
    if (tempo <= 0) {
      if (!cooperativeDelay(noteDuration * 0.30)) {
        break;
      }
    }
  }

  noTone(BUZZER_PIN);
  digitalWrite(LED_BUILTIN, HIGH);
  playing = false;
  stopRequested = false;
}

}  // namespace

namespace SoundPlayer {

bool begin() {
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
  return playing;
}

void stop() {
  if (playing) {
    stopRequested = true;
    noTone(BUZZER_PIN);
    digitalWrite(LED_BUILTIN, HIGH);
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
