#include "web_portal.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <LittleFS.h>

#include "sound_player.h"
#include "log.h"

namespace {

const char AP_SSID[] = "SoundCloc";
// WPA2 requires a password of at least eight characters.
const char AP_PASSWORD[] = "12345678";

DNSServer dnsServer;
ESP8266WebServer webServer(80);
WiFiEventHandler stationConnectedHandler;
WiFiEventHandler stationDisconnectedHandler;

String macAddress(const uint8_t *mac) {
  char text[18];
  snprintf(text, sizeof(text), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(text);
}

void handleStationConnected(const WiFiEventSoftAPModeStationConnected &event) {
  String mac = macAddress(event.mac);
  LOG_PRINTF("Portal client connected: %s (station %u)", mac.c_str(), event.aid);
}

void handleStationDisconnected(const WiFiEventSoftAPModeStationDisconnected &event) {
  String mac = macAddress(event.mac);
  LOG_PRINTF("Portal client disconnected: %s (station %u)", mac.c_str(), event.aid);
}

const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>SoundCloc</title>
  <style>
    :root { font-family: system-ui, sans-serif; color-scheme: dark; }
    body { margin: 0; min-height: 100vh; display: grid; place-items: center;
           background: #101827; color: #f8fafc; }
    main { width: min(28rem, calc(100% - 2rem)); padding: 2rem;
           box-sizing: border-box; border-radius: 1rem; background: #1e293b;
           box-shadow: 0 1rem 3rem #0006; }
    .build-time { margin: 2rem 0 0; color: #94a3b8; font-size: .72rem;
                  text-align: center; }
    h1 { margin-top: 0; }
    label { display: block; margin-bottom: .5rem; color: #cbd5e1; }
    select, button { width: 100%; box-sizing: border-box; padding: .85rem;
                     border: 0; border-radius: .6rem; font-size: 1rem; }
    select { background: #f8fafc; color: #0f172a; }
    button { margin-top: 1rem; background: #38bdf8; color: #082f49;
             font-weight: 700; cursor: pointer; }
    button:disabled { opacity: .55; cursor: wait; }
    #status { min-height: 1.5em; color: #bae6fd; }
    h2 { margin-top: 2rem; }
    table { width: 100%; border-collapse: collapse; }
    th, td { padding: .65rem .35rem; border-bottom: 1px solid #475569;
             text-align: left; }
    th:last-child, td:last-child { text-align: right; }
    .file { padding: 0; margin: 0; width: auto; color: #7dd3fc;
            background: none; font-weight: 600; text-align: left; }
    .totals td { color: #94a3b8; border: 0; }
    #viewer { display: none; margin-top: 1rem; }
    #viewer.open { display: block; }
    #viewer pre { max-height: 22rem; overflow: auto; padding: 1rem;
                  border-radius: .6rem; background: #0f172a; white-space: pre-wrap;
                  overflow-wrap: anywhere; }
    #viewer img { display: block; max-width: 100%; max-height: 22rem;
                  margin: auto; }
  </style>
</head>
<body>
  <main>
    <h1>SoundCloc</h1>
    <label for="sound">Choose a sound</label>
    <select id="sound"><option>Loading sounds…</option></select>
    <button id="play" disabled>Play</button>
    <p id="status"></p>
    <h2>Files <small id="directory-path">/</small></h2>
    <table>
      <thead><tr><th>Filename</th><th>Size</th></tr></thead>
      <tbody id="files"><tr><td colspan="2">Loading files…</td></tr></tbody>
      <tfoot id="totals" class="totals"></tfoot>
    </table>
    <section id="viewer">
      <h2 id="viewer-title"></h2>
      <div id="viewer-content"></div>
    </section>
    <p id="build-time" class="build-time">Firmware build: loading…</p>
  </main>
  <script>
    const sound = document.querySelector('#sound');
    const play = document.querySelector('#play');
    const status = document.querySelector('#status');
    const buildTime = document.querySelector('#build-time');
    const files = document.querySelector('#files');
    const totals = document.querySelector('#totals');
    const directoryPath = document.querySelector('#directory-path');
    const viewer = document.querySelector('#viewer');
    const viewerTitle = document.querySelector('#viewer-title');
    const viewerContent = document.querySelector('#viewer-content');
    let currentDirectory = '/';

    function formatBytes(bytes) {
      return Number(bytes).toLocaleString() + ' bytes';
    }

    async function loadSounds() {
      try {
        const response = await fetch('/api/sounds');
        if (!response.ok) throw new Error(await response.text());
        const data = await response.json();
        sound.replaceChildren(...data.sounds.map(({name}) => {
          const option = document.createElement('option');
          option.value = option.textContent = name;
          return option;
        }));
        play.disabled = data.sounds.length === 0;
        status.textContent = data.sounds.length ? '' : 'No sounds found.';
        if (data.sounds.length) await updateSelection();
      } catch (error) {
        status.textContent = 'Could not load sounds: ' + error.message;
      }
    }

    async function updateSelection() {
      const response = await fetch('/api/select', {
        method: 'POST',
        headers: {'Content-Type': 'application/x-www-form-urlencoded'},
        body: new URLSearchParams({name: sound.value})
      });
      if (!response.ok) throw new Error(await response.text());
    }

    sound.addEventListener('change', async () => {
      try {
        await updateSelection();
        status.textContent = 'Selected ' + sound.value + '.';
      } catch (error) {
        status.textContent = 'Could not select sound: ' + error.message;
      }
    });

    async function loadBuildTime() {
      try {
        const response = await fetch('/api/build');
        if (!response.ok) throw new Error();
        const data = await response.json();
        buildTime.textContent = 'Firmware build: ' + data.build;
      } catch (_) {
        buildTime.textContent = 'Firmware build time unavailable';
      }
    }

    play.addEventListener('click', async () => {
      play.disabled = true;
      status.textContent = 'Playing…';
      try {
        const response = await fetch('/api/play', {
          method: 'POST',
          headers: {'Content-Type': 'application/x-www-form-urlencoded'},
          body: new URLSearchParams({name: sound.value})
        });
        if (!response.ok) throw new Error(await response.text());
        status.textContent = 'Finished playing ' + sound.value + '.';
      } catch (error) {
        status.textContent = 'Could not play sound: ' + error.message;
      } finally {
        play.disabled = false;
      }
    });

    async function displayFile(name) {
      viewer.classList.add('open');
      viewerTitle.textContent = name;
      viewerContent.textContent = 'Loading…';
      const url = '/api/file?name=' + encodeURIComponent(name);
      const extension = name.split('.').pop().toLowerCase();

      if (/^(png|jpg|jpeg|gif|svg)$/.test(extension)) {
        const image = document.createElement('img');
        image.src = url;
        image.alt = name;
        viewerContent.replaceChildren(image);
        return;
      }

      try {
        const response = await fetch(url);
        if (!response.ok) throw new Error(await response.text());
        let content = await response.text();
        if (extension === 'json') {
          try { content = JSON.stringify(JSON.parse(content), null, 2); } catch (_) {}
        }
        const pre = document.createElement('pre');
        pre.textContent = content;
        viewerContent.replaceChildren(pre);
      } catch (error) {
        viewerContent.textContent = 'Could not display file: ' + error.message;
      }
    }

    function parentDirectory(path) {
      if (path === '/') return '/';
      const trimmed = path.replace(/\/$/, '');
      const slash = trimmed.lastIndexOf('/');
      return slash <= 0 ? '/' : trimmed.substring(0, slash);
    }

    function baseName(path) {
      const trimmed = path.replace(/\/$/, '');
      return trimmed.substring(trimmed.lastIndexOf('/') + 1);
    }

    async function loadFiles(path = '/') {
      try {
        const response = await fetch('/api/files?path=' + encodeURIComponent(path));
        if (!response.ok) throw new Error(await response.text());
        const data = await response.json();
        currentDirectory = data.path;
        directoryPath.textContent = currentDirectory;
        viewer.classList.remove('open');

        const entries = data.files.slice();
        if (currentDirectory !== '/') {
          entries.unshift({
            name: '..',
            path: parentDirectory(currentDirectory),
            directory: true,
            size: 0
          });
        }

        files.replaceChildren(...entries.map(file => {
          const row = document.createElement('tr');
          const nameCell = document.createElement('td');
          const nameButton = document.createElement('button');
          nameButton.className = 'file';
          nameButton.textContent =
            file.name === '..' ? '..' : baseName(file.path) + (file.directory ? '/' : '');
          nameButton.addEventListener('click', () => {
            if (file.directory) loadFiles(file.path);
            else displayFile(file.path);
          });
          nameCell.appendChild(nameButton);
          const sizeCell = document.createElement('td');
          sizeCell.textContent = file.directory ? '' : Number(file.size).toLocaleString();
          row.append(nameCell, sizeCell);
          return row;
        }));
        if (!entries.length) {
          files.innerHTML = '<tr><td colspan="2">No files found</td></tr>';
        }
        if (data.total !== undefined) {
          totals.innerHTML =
            '<tr><td>Used</td><td>' + formatBytes(data.used) + '</td></tr>' +
            '<tr><td>Available</td><td>' +
            formatBytes(data.total - data.used) + '</td></tr>';
        }
      } catch (error) {
        files.innerHTML = '<tr><td colspan="2">Could not load files</td></tr>';
      }
    }

    loadSounds();
    loadFiles('/');
    loadBuildTime();
  </script>
</body>
</html>
)HTML";

void handleSoundList() {
  String json;
  if (!SoundPlayer::namesAsJson(json)) {
    webServer.send(500, "text/plain", "Unable to read sounds.json");
    return;
  }
  webServer.send(200, "application/json", json);
}

void handlePlay() {
  if (!webServer.hasArg("name") || webServer.arg("name").isEmpty()) {
    webServer.send(400, "text/plain", "Missing sound name");
    return;
  }

  if (!SoundPlayer::select(webServer.arg("name")) ||
      !SoundPlayer::playSelected()) {
    webServer.send(404, "text/plain", "Sound not found or sounds.json is invalid");
    return;
  }
  webServer.send(200, "application/json", "{\"ok\":true}");
}

void handleSelect() {
  if (!webServer.hasArg("name") || webServer.arg("name").isEmpty()) {
    webServer.send(400, "text/plain", "Missing sound name");
    return;
  }
  if (!SoundPlayer::select(webServer.arg("name"))) {
    webServer.send(404, "text/plain", "Sound not found");
    return;
  }
  webServer.send(200, "application/json", "{\"ok\":true}");
}

void handleBuildTime() {
  webServer.send(200, "application/json",
                 String("{\"build\":\"") + __DATE__ + " " + __TIME__ + "\"}");
}

String normalizedFilePath(const String &requestedName) {
  if (requestedName.isEmpty() || requestedName.indexOf("..") >= 0 ||
      requestedName.indexOf('\\') >= 0) {
    return String();
  }

  String path = requestedName;
  if (!path.startsWith("/")) {
    path = "/" + path;
  }
  if (path.length() <= 1 || path.endsWith("/")) {
    return String();
  }
  return path;
}

const char *mimeTypeForPath(const String &path) {
  if (path.endsWith(".json")) return "application/json";
  if (path.endsWith(".txt") || path.endsWith(".log") || path.endsWith(".csv")) {
    return "text/plain";
  }
  if (path.endsWith(".html") || path.endsWith(".htm")) return "text/html";
  if (path.endsWith(".css")) return "text/css";
  if (path.endsWith(".js")) return "application/javascript";
  if (path.endsWith(".png")) return "image/png";
  if (path.endsWith(".jpg") || path.endsWith(".jpeg")) return "image/jpeg";
  if (path.endsWith(".gif")) return "image/gif";
  if (path.endsWith(".svg")) return "image/svg+xml";
  return "application/octet-stream";
}

void handleListFiles() {
  String path = webServer.hasArg("path") ? webServer.arg("path") : "/";
  if (path.isEmpty()) {
    path = "/";
  }
  if (!path.startsWith("/") || path.indexOf("..") >= 0 ||
      path.indexOf('\\') >= 0) {
    webServer.send(400, "text/plain", "Invalid directory path");
    return;
  }
  while (path.length() > 1 && path.endsWith("/")) {
    path.remove(path.length() - 1);
  }

  if (path != "/") {
    File requestedDirectory = LittleFS.open(path, "r");
    if (!requestedDirectory || !requestedDirectory.isDirectory()) {
      requestedDirectory.close();
      webServer.send(404, "text/plain", "Directory not found");
      return;
    }
    requestedDirectory.close();
  }

  JsonDocument response;
  response["path"] = path;
  JsonArray files = response["files"].to<JsonArray>();
  Dir directory = LittleFS.openDir(path);
  while (directory.next()) {
    String entryName = directory.fileName();
    String entryPath =
        path == "/" ? "/" + entryName : path + "/" + entryName;
    bool isDirectory = directory.isDirectory();

    // LittleFS directory names are relative to the opened directory. Verify
    // the entry itself as well so directory navigation never depends only on
    // the iterator's type flag.
    File entry = LittleFS.open(entryPath, "r");
    if (entry) {
      isDirectory = entry.isDirectory();
      entry.close();
    }

    JsonObject item = files.add<JsonObject>();
    item["name"] = entryName;
    item["path"] = entryPath;
    item["directory"] = isDirectory;
    item["size"] = isDirectory ? 0 : directory.fileSize();
    yield();
  }

  FSInfo info;
  if (LittleFS.info(info)) {
    response["total"] = info.totalBytes;
    response["used"] = info.usedBytes;
  }

  String json;
  serializeJson(response, json);
  webServer.send(200, "application/json", json);
}

void handleReadFile() {
  String path = normalizedFilePath(webServer.arg("name"));
  if (path.isEmpty()) {
    webServer.send(400, "text/plain", "Invalid file name");
    return;
  }

  File file = LittleFS.open(path, "r");
  if (!file) {
    webServer.send(404, "text/plain", "File not found");
    return;
  }

  webServer.streamFile(file, mimeTypeForPath(path));
  file.close();
}

void redirectToPortal() {
  webServer.sendHeader("Location", String("http://") + WiFi.softAPIP().toString(), true);
  webServer.send(302, "text/plain", "");
}

}  // namespace

namespace WebPortal {

void begin() {
  stationConnectedHandler = WiFi.onSoftAPModeStationConnected(handleStationConnected);
  stationDisconnectedHandler =
      WiFi.onSoftAPModeStationDisconnected(handleStationDisconnected);

  WiFi.mode(WIFI_AP);
  if (!WiFi.softAP(AP_SSID, AP_PASSWORD)) {
    LOG_PRINTLN("Failed to start access point");
    return;
  }

  dnsServer.start(53, "*", WiFi.softAPIP());
  webServer.on("/", HTTP_GET, []() {
    webServer.send_P(200, "text/html", INDEX_HTML);
  });
  webServer.on("/api/sounds", HTTP_GET, handleSoundList);
  webServer.on("/api/select", HTTP_POST, handleSelect);
  webServer.on("/api/play", HTTP_POST, handlePlay);
  webServer.on("/api/build", HTTP_GET, handleBuildTime);
  webServer.on("/api/files", HTTP_GET, handleListFiles);
  webServer.on("/api/file", HTTP_GET, handleReadFile);
  webServer.on("/generate_204", redirectToPortal);
  webServer.on("/hotspot-detect.html", redirectToPortal);
  webServer.on("/connecttest.txt", redirectToPortal);
  webServer.onNotFound(redirectToPortal);
  webServer.begin();

  String address = WiFi.softAPIP().toString();
  LOG_PRINTF("SoundCloc portal: http://%s", address.c_str());
}

void tick() {
  dnsServer.processNextRequest();
  webServer.handleClient();
}

}  // namespace WebPortal
