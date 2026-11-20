# Befehlsreferenz: Bibliotheken und APIs

Nachschlagewerk für alle Befehle, die das Projekt braucht. Jeder Eintrag hat eine kurze Beschreibung und ein kleines Beispiel. Die Beispiele zeigen nur den einzelnen Befehl; wie ihr ihn in eure Komponenten einbaut, entscheidet ihr selbst.

Geprüft gegen die Versionen in `.pio/libdeps/esp32dev` (Stand 20.11.2026):

| Bibliothek | Version | Wird gebraucht in |
|------------|---------|-------------------|
| [ESP32-A2DP](https://github.com/pschatzmann/ESP32-A2DP) | 1.8.12 | `bt_sink` |
| [arduino-audio-tools](https://github.com/pschatzmann/arduino-audio-tools) | 1.3.0 | `audio_output` (und `bt_sink`) |
| [arduino-audio-driver](https://github.com/pschatzmann/arduino-audio-driver) | 0.3.2 | `audio_output` (indirekt über AudioTools) |
| Arduino-ESP32-Core (WiFi, HTTPClient) | 3.x (ESP-IDF 5) | `wifi_manager`, `spotify_client` |
| [ArduinoJson](https://arduinojson.org/) | 7.x | `spotify_client` |
| Spotify Web API | v1 | `spotify_client` |

Schnittstellen und Zustände: [`interfaces.md`](interfaces.md), [`uml/uml.md`](uml/uml.md)

## Inhalt

1. [ESP32-A2DP (Bluetooth-Empfänger)](#1-esp32-a2dp-bluetooth-empfänger)
2. [Arduino Audio Tools (I2S, Streams, Puffer)](#2-arduino-audio-tools-i2s-streams-puffer)
3. [Arduino Audio Driver (Codec ES8388)](#3-arduino-audio-driver-codec-es8388)
4. [ESP32 WiFi](#4-esp32-wifi)
5. [HTTPS und JSON (Grundlage für Spotify)](#5-https-und-json-grundlage-für-spotify)
6. [Spotify Web API](#6-spotify-web-api)
7. [ESP32, FreeRTOS und Hilfsmittel](#7-esp32-freertos-und-hilfsmittel)
8. [Unit-Tests mit Unity](#8-unit-tests-mit-unity)
9. [PlatformIO-Befehle](#9-platformio-befehle)
10. [Stolperfallen](#10-stolperfallen)

---

## 1. ESP32-A2DP (Bluetooth-Empfänger)

Macht den ESP32 zu einem Bluetooth-Lautsprecher (A2DP-Sink). Das Smartphone schickt komprimiertes Audio (SBC), die Bibliothek dekodiert es zu 16-Bit-Stereo-PCM. Steuerbefehle und Metadaten laufen über AVRCP („AVRC“ in den Funktionsnamen).

### Einbinden und Objekt anlegen

```cpp
#include "AudioTools.h"          // vor BluetoothA2DPSink.h, aktiviert die AudioTools-Ausgabe
#include "BluetoothA2DPSink.h"

BluetoothA2DPSink a2dp_sink;     // ohne Ausgabe: Daten nur per Callback
```

Varianten des Konstruktors:

| Aufruf | Bedeutung |
|--------|-----------|
| `BluetoothA2DPSink a2dp_sink;` | keine feste Ausgabe; Daten über `set_stream_reader()` abholen |
| `BluetoothA2DPSink a2dp_sink(kit);` | schreibt direkt in einen AudioTools-Stream (z. B. `AudioBoardStream`) |
| `BluetoothA2DPSink a2dp_sink(Serial);` | schreibt in jedes Arduino-`Print`-Objekt |

> Für das Projekt passt die erste Variante: `bt_sink` holt die Daten per Callback und reicht sie an `audio_output_write()` weiter. Dann gehört I2S allein `audio_output`.

### `start(name)` / `start(name, auto_reconnect)`

Startet Bluetooth und macht den ESP32 unter dem Namen sichtbar. Ohne zweiten Parameter verbindet er sich nach dem Neustart automatisch wieder mit dem letzten Gerät.

```cpp
a2dp_sink.start("ESP32 Audio Adapter");          // mit Auto-Reconnect
a2dp_sink.start("ESP32 Audio Adapter", false);   // ohne Auto-Reconnect
```

Alle `set_...`-Callbacks **vor** `start()` setzen.

### `end(release_memory)`

Beendet Bluetooth. Mit `true` wird auch der Speicher des BT-Controllers freigegeben; danach ist **kein erneutes `start()` möglich** (nur noch per Neustart).

```cpp
a2dp_sink.end();       // stoppen, später wieder start() möglich
a2dp_sink.end(true);   // endgültig, gibt ca. 30–70 KB RAM frei
```

### `set_stream_reader(callback, i2s_output)`

Wichtigster Callback: Liefert die dekodierten PCM-Daten (16 Bit, Stereo verschachtelt, Lautstärke schon angewendet). Mit `i2s_output = false` schreibt die Bibliothek nicht selbst auf I2S.

```cpp
void on_bt_data(const uint8_t *data, uint32_t len) {
    // data = L, R, L, R ... als int16_t; len in Bytes
    // hier z. B. weiterreichen, NICHT lange blockieren
}

a2dp_sink.set_stream_reader(on_bt_data, false);
```

Läuft im Bluetooth-Task. Kein `Serial.print()` pro Paket, kein `delay()`.

### `set_raw_stream_reader(callback)`

Wie `set_stream_reader`, aber **vor** der Software-Lautstärke der Bibliothek. Nützlich, wenn die Lautstärke nur am Codec geregelt werden soll.

```cpp
void on_raw_data(const uint8_t *data, uint32_t len) { /* ... */ }
a2dp_sink.set_raw_stream_reader(on_raw_data);
```

### `set_on_data_received(callback)`

Wird bei jedem Audiopaket aufgerufen, ohne die Daten. Gut als „es kommt Audio an“-Signal oder für einen Watchdog.

```cpp
volatile uint32_t last_bt_data_ms = 0;
void on_data() { last_bt_data_ms = millis(); }

a2dp_sink.set_on_data_received(on_data);
```

### `set_sample_rate_callback(callback)`

Meldet die Abtastrate, die das Smartphone gewählt hat (meist 44100, manchmal 48000).

```cpp
void on_rate(uint16_t rate) {
    Serial.printf("BT-Abtastrate: %u Hz\n", rate);
}
a2dp_sink.set_sample_rate_callback(on_rate);
```

### `set_on_connection_state_changed(callback, obj)`

Meldet Verbindungsänderungen. Damit erzeugt man `EVT_BT_CONNECTED` / `EVT_BT_DISCONNECTED`.

| Zustand (`esp_a2d_connection_state_t`) | Bedeutung |
|-----------------------------------------|-----------|
| `ESP_A2D_CONNECTION_STATE_DISCONNECTED` | getrennt |
| `ESP_A2D_CONNECTION_STATE_CONNECTING` | baut auf |
| `ESP_A2D_CONNECTION_STATE_CONNECTED` | verbunden |
| `ESP_A2D_CONNECTION_STATE_DISCONNECTING` | trennt |

```cpp
void on_conn(esp_a2d_connection_state_t state, void *obj) {
    if (state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
        // verbunden
    } else if (state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
        // getrennt
    }
}
a2dp_sink.set_on_connection_state_changed(on_conn);
```

Der zweite Parameter `obj` ist ein frei wählbarer Zeiger, der an den Callback durchgereicht wird (meist `nullptr`).

### `set_on_audio_state_changed(callback, obj)`

Meldet, ob gerade Audio läuft (`ESP_A2D_AUDIO_STATE_STARTED`) oder pausiert/gestoppt ist. Unabhängig von der Verbindung: Ein verbundenes Handy kann pausieren.

```cpp
void on_audio(esp_a2d_audio_state_t state, void *obj) {
    bool playing = (state == ESP_A2D_AUDIO_STATE_STARTED);
    Serial.println(playing ? "BT spielt" : "BT pausiert");
}
a2dp_sink.set_on_audio_state_changed(on_audio);
```

`set_on_audio_state_changed_post()` hat dieselbe Signatur und wird erst aufgerufen, nachdem die Bibliothek I2S umgeschaltet hat.

### Status abfragen

| Funktion | Rückgabe |
|----------|----------|
| `is_connected()` | `true`, wenn A2DP verbunden |
| `get_connection_state()` | `esp_a2d_connection_state_t` |
| `get_audio_state()` | `esp_a2d_audio_state_t` |
| `is_avrc_connected()` | `true`, wenn Fernsteuerung (AVRCP) verbunden |
| `get_peer_name()` | Name des verbundenen Geräts |
| `get_name()` | eigener Bluetooth-Name |

```cpp
if (a2dp_sink.is_connected()) {
    Serial.printf("Verbunden mit %s\n", a2dp_sink.get_peer_name());
}
```

### `to_str(state)`

Wandelt Zustände und Adressen in lesbaren Text, praktisch fürs Logging.

```cpp
Serial.println(a2dp_sink.to_str(a2dp_sink.get_connection_state()));
```

### Verbindung steuern

| Funktion | Beschreibung |
|----------|--------------|
| `set_auto_reconnect(bool)` | automatisches Wiederverbinden mit dem letzten Gerät an/aus |
| `reconnect()` | mit dem zuletzt verbundenen Gerät verbinden |
| `disconnect()` | aktuelle Verbindung trennen, Bluetooth bleibt an |
| `clean_last_connection()` | gespeichertes letztes Gerät löschen |
| `set_connectable(bool)` | Verbindungen annehmen ja/nein |
| `set_discoverability(mode)` | sichtbar (`ESP_BT_GENERAL_DISCOVERABLE`) oder unsichtbar (`ESP_BT_NON_DISCOVERABLE`) |

```cpp
a2dp_sink.disconnect();                                   // Handy rauswerfen
a2dp_sink.set_discoverability(ESP_BT_NON_DISCOVERABLE);   // nicht mehr in der Geräteliste
```

### `set_output_active(bool)`

Schaltet die Ausgabe der Bibliothek an oder aus, ohne die Verbindung zu trennen. Nur relevant, wenn die Bibliothek selbst ausgibt (Konstruktor mit Stream).

```cpp
a2dp_sink.set_output_active(false);   // stumm, Verbindung bleibt
```

### Lautstärke

Bluetooth-Lautstärke läuft von **0 bis 127**. Laut [`interfaces.md`](interfaces.md) muss `bt_sink` auf 0–100 % umrechnen.

| Funktion | Beschreibung |
|----------|--------------|
| `set_avrc_rn_volumechange(cb)` | Callback, wenn das Handy die Lautstärke ändert (Wert 0–127) |
| `set_volume(uint8_t)` | eigene Lautstärke setzen (0–127), wird ans Handy gemeldet |
| `get_volume()` | aktuelle Lautstärke (0–127) |
| `volume_up()` / `volume_down()` | Lautstärke-Taste ans Handy senden |

```cpp
void on_volume(int vol127) {
    uint8_t percent = (vol127 * 100) / 127;
    Serial.printf("Lautstärke %d %%\n", percent);
}
a2dp_sink.set_avrc_rn_volumechange(on_volume);
```

> Achtung: `set_stream_reader` liefert Daten, auf die die Bibliothek ihre Lautstärke schon angewendet hat. Regelt ihr zusätzlich den Codec, wird es doppelt leiser. Entweder `set_raw_stream_reader` nehmen oder nur eine der beiden Stellen regeln.

### Wiedergabe steuern (AVRCP)

Schickt Tastenbefehle ans Handy. Funktioniert nur, wenn `is_avrc_connected()` true ist.

| Funktion | Befehl |
|----------|--------|
| `play()` | Wiedergabe |
| `pause()` | Pause |
| `stop()` | Stopp |
| `next()` | nächster Titel |
| `previous()` | vorheriger Titel |
| `fast_forward()` / `rewind()` | vor- / zurückspulen |

```cpp
if (a2dp_sink.is_avrc_connected()) a2dp_sink.next();
```

### Metadaten (Titel, Interpret)

`set_avrc_metadata_attribute_mask()` legt fest, welche Felder kommen; `set_avrc_metadata_callback()` empfängt sie.

```cpp
void on_meta(uint8_t id, const uint8_t *text) {
    if (id == ESP_AVRC_MD_ATTR_TITLE)  Serial.printf("Titel: %s\n", text);
    if (id == ESP_AVRC_MD_ATTR_ARTIST) Serial.printf("Interpret: %s\n", text);
}

a2dp_sink.set_avrc_metadata_attribute_mask(ESP_AVRC_MD_ATTR_TITLE | ESP_AVRC_MD_ATTR_ARTIST);
a2dp_sink.set_avrc_metadata_callback(on_meta);
```

Weitere Felder: `ESP_AVRC_MD_ATTR_ALBUM`, `ESP_AVRC_MD_ATTR_PLAYING_TIME`, `ESP_AVRC_MD_ATTR_TRACK_NUM`, `ESP_AVRC_MD_ATTR_GENRE`.

### `set_avrc_rn_playstatus_callback(callback)`

Meldet Play/Pause-Status direkt vom Handy (`ESP_AVRC_PLAYBACK_PLAYING`, `ESP_AVRC_PLAYBACK_PAUSED`, `ESP_AVRC_PLAYBACK_STOPPED`).

```cpp
void on_playstatus(esp_avrc_playback_stat_t s) {
    Serial.println(s == ESP_AVRC_PLAYBACK_PLAYING ? "Play" : "Pause/Stopp");
}
a2dp_sink.set_avrc_rn_playstatus_callback(on_playstatus);
```

### `set_avrc_connection_state_callback(callback)`

Meldet, wann AVRCP verbunden oder getrennt ist. Erst danach funktionieren `play()`, `next()` usw.

```cpp
void on_avrc(bool connected) { Serial.printf("AVRC: %d\n", connected); }
a2dp_sink.set_avrc_connection_state_callback(on_avrc);
```

### `set_address_validator(callback)`

Erlaubt nur bestimmte Geräte. Rückgabe `false` lehnt die Verbindung ab.

```cpp
bool allow_device(esp_bd_addr_t addr) {
    return true;   // z. B. addr mit einer erlaubten MAC vergleichen
}
a2dp_sink.set_address_validator(allow_device);
```

### PIN-Kopplung (optional)

```cpp
a2dp_sink.activate_pin_code(true);   // vor start()
// später, wenn das Handy fragt:
a2dp_sink.confirm_pin_code();
```

### Task-Einstellungen

Legt fest, auf welchem CPU-Kern und mit welcher Priorität der Bluetooth-Task läuft. Vor `start()` aufrufen.

```cpp
a2dp_sink.set_task_core(1);          // 0 oder 1
a2dp_sink.set_task_priority(10);
```

### `BluetoothA2DPSinkQueued`

Variante mit eigenem Puffer und eigenem Ausgabetask. Hilft bei Aussetzern, wenn die Ausgabe zeitweise langsam ist. Gleiche Funktionen wie `BluetoothA2DPSink`, zusätzlich:

```cpp
#include "BluetoothA2DPSinkQueued.h"
BluetoothA2DPSinkQueued a2dp_sink(kit);
a2dp_sink.set_i2s_ringbuffer_size(8 * 1024);
a2dp_sink.set_i2s_stack_size(4096);
```

---

## 2. Arduino Audio Tools (I2S, Streams, Puffer)

Baukasten für Audio: Quellen, Ziele und Filter sind „Streams“, die man mit `write()` oder `StreamCopy` verbindet. Für das Audio Kit wichtig ist vor allem `AudioBoardStream`: I2S und Codec in einem Objekt.

### Einbinden

```cpp
#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"   // braucht arduino-audio-driver
```

### `AudioInfo`

Beschreibt das Audioformat: Abtastrate, Kanäle, Bits pro Sample.

```cpp
AudioInfo info(44100, 2, 16);   // 44,1 kHz, Stereo, 16 Bit
Serial.println(info.sample_rate);
```

### `AudioBoardStream`

I2S-Ausgang plus Codec-Ansteuerung für ein bekanntes Board.

```cpp
AudioBoardStream kit(AudioKitEs8388V1);   // oder AudioKitEs8388V2, siehe Abschnitt 3
```

### `defaultConfig(mode)`

Liefert eine Grundkonfiguration, die man anpasst und an `begin()` übergibt. `TX_MODE` = nur Ausgabe, `RX_MODE` = nur Eingang, `RXTX_MODE` = beides.

```cpp
auto cfg = kit.defaultConfig(TX_MODE);
cfg.sample_rate     = 44100;
cfg.channels        = 2;
cfg.bits_per_sample = 16;
cfg.sd_active       = false;   // SD-Karte aus, sonst belegt sie Pins (z. B. GPIO 13)
cfg.buffer_count    = 8;       // Anzahl DMA-Puffer
cfg.buffer_size     = 512;     // Bytes pro DMA-Puffer
```

| Feld | Bedeutung |
|------|-----------|
| `sample_rate`, `channels`, `bits_per_sample` | Audioformat |
| `buffer_count`, `buffer_size` | DMA-Puffer; mehr = robuster gegen Aussetzer, aber mehr Latenz und RAM |
| `sd_active` | SD-Kartenleser initialisieren (für uns `false`) |
| `output_device` | `DAC_OUTPUT_ALL` (Standard), `DAC_OUTPUT_LINE1`, `DAC_OUTPUT_LINE2` |
| `use_apll` | genauerer Takt für 44,1 kHz |
| `auto_clear` | Stille ausgeben, wenn keine Daten kommen |

### `begin(cfg)` / `end()`

Startet I2S und Codec bzw. schaltet beides ab. `begin()` gibt `false` zurück, wenn der Codec nicht antwortet (falsches Board-Objekt, I2C-Pins).

```cpp
if (!kit.begin(cfg)) {
    Serial.println("Codec-Start fehlgeschlagen");
}
kit.end();
```

### `write(data, len)`

Schreibt PCM-Bytes in den I2S-Puffer. Blockiert, bis Platz ist. Rückgabe: geschriebene Bytes.

```cpp
int16_t samples[256 * 2];   // 256 Stereo-Frames
size_t written = kit.write((uint8_t *)samples, sizeof(samples));
```

### `setAudioInfo(info)`

Ändert die Abtastrate im laufenden Betrieb (I2S und Codec).

```cpp
kit.setAudioInfo(AudioInfo(48000, 2, 16));
```

### `audioInfo()`

Liefert das aktuell eingestellte Format.

```cpp
Serial.println(kit.audioInfo().sample_rate);
```

### `setVolume(float)` / `volume()`

Lautstärke am Codec, **0.0 bis 1.0**.

```cpp
kit.setVolume(0.5);              // 50 %
float v = kit.volume();
kit.setVolume(percent / 100.0f); // aus Prozent umrechnen
```

### `setMute(bool)`

Codec stumm schalten, ohne I2S zu stoppen. Gut beim Quellenwechsel gegen Knacken.

```cpp
kit.setMute(true);
kit.setMute(false);
```

### `setPAPower(bool)` / `setSpeakerActive(bool)`

Schaltet den eingebauten Lautsprecher-Verstärker (GPIO 21) an oder aus. Für reinen Kopfhörerbetrieb aus lassen spart Strom und Rauschen.

```cpp
kit.setPAPower(false);
```

### `headphoneStatus()`

`true`, wenn ein Kopfhörer eingesteckt ist (Klinken-Erkennung, GPIO 39).

```cpp
if (kit.headphoneStatus()) Serial.println("Kopfhörer steckt");
```

### Tasten des Audio Kits

`addDefaultActions()` legt Lautstärke-, Start/Stopp- und Kopfhörer-Aktionen auf die Board-Tasten; `processActions()` muss dann regelmäßig in `loop()` laufen. Eigene Aktionen mit `addAction()`.

```cpp
void on_key(bool active, digital_pin_t pin, void *ref) { Serial.println("Taste"); }

kit.addDefaultActions();
kit.addAction(AudioDriverKey::KEY_PLAY, on_key);   // oder direkt eine Pin-Nummer: kit.addAction(36, on_key);
// in loop():
kit.processActions();
```

> Die Board-Tasten teilen sich Pins mit anderen Funktionen (z. B. KEY2 = GPIO 13 = SD-Karte). Nur Tasten nutzen, die frei sind.

### `I2SStream` (ohne Codec)

Reines I2S, wenn man den Codec separat ansteuert oder einen externen DAC nimmt.

```cpp
I2SStream i2s;
auto icfg = i2s.defaultConfig(TX_MODE);
icfg.pin_bck = 27; icfg.pin_ws = 25; icfg.pin_data = 26;
i2s.begin(icfg);
```

### Testton: `SineWaveGenerator` + `GeneratedSoundStream` + `StreamCopy`

Erzeugt einen Sinuston. Ideal, um `audio_output` ohne Bluetooth/Spotify zu testen.

```cpp
AudioInfo info(44100, 2, 16);
SineWaveGenerator<int16_t> sine(16000);         // Amplitude (max. 32767)
GeneratedSoundStream<int16_t> sound(sine);
StreamCopy copier(kit, sound);                  // Ziel, Quelle

void setup() {
    // kit.begin(cfg) wie oben
    sine.begin(info, 440.0);                     // 440 Hz
}
void loop() {
    copier.copy();                               // kopiert einen Puffer
}
```

### `StreamCopy`

Kopiert von einer Quelle in ein Ziel. `copy()` kopiert einen Puffer und kehrt zurück; `copyAll()` kopiert bis zum Ende der Quelle.

```cpp
StreamCopy copier(ziel, quelle, 1024);   // 1024 = Puffergröße in Bytes
size_t n = copier.copy();
```

### `VolumeStream`

Software-Lautstärke als Filter zwischen Quelle und Ziel. Alternative zur Codec-Lautstärke.

```cpp
VolumeStream volume(kit);       // schreibt weiter in kit
volume.begin(info);
volume.setVolume(0.3);          // 0.0–1.0
volume.write(data, len);
```

### `CsvOutput`

Gibt Samples als Zahlen auf der seriellen Konsole aus. Zum Prüfen, ob überhaupt Daten ankommen (Serial Plotter).

```cpp
CsvOutput<int16_t> csv(Serial);
csv.begin(info);
csv.write(data, len);
```

### `MeasuringStream`

Misst den Durchsatz (Bytes pro Sekunde) und schreibt weiter ins Ziel. Gut, um zu sehen, ob Spotify/BT schnell genug liefern.

```cpp
MeasuringStream measure(kit);
measure.begin(info);
measure.write(data, len);
Serial.println(measure.bytesPerSecond());
```

### `BufferRTOS` (Ringpuffer zwischen Tasks)

Thread-sicherer Puffer: Ein Task schreibt, ein anderer liest. Entkoppelt z. B. Netzwerk-Task und Audio-Ausgabe.

```cpp
BufferRTOS<uint8_t> buffer(16 * 1024);        // 16 KB

buffer.writeArray(data, len);                 // Task A
size_t n = buffer.readArray(out, sizeof(out)); // Task B
size_t frei = buffer.availableForWrite();
buffer.reset();                               // leeren
```

### `QueueStream`

Macht aus einem Puffer einen Stream, damit man ihn mit `StreamCopy` benutzen kann.

```cpp
QueueStream<uint8_t> queue(buffer);
queue.begin();
StreamCopy copier(kit, queue);
```

### `MutexRTOS` + `LockGuard`

Schützt gemeinsame Daten, wenn mehrere Tasks darauf zugreifen. `LockGuard` gibt den Mutex am Ende des Blocks automatisch frei.

```cpp
MutexRTOS mutex;
void set_value(int v) {
    LockGuard guard(mutex);
    shared_value = v;
}
```

### Logging

```cpp
AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);   // Debug, Info, Warning, Error
```

Hohe Log-Stufen (Info/Debug) verursachen Aussetzer. Im Betrieb `Warning` oder `Error`.

### Rückfallebene: Internetradio

Falls cspot nicht läuft (siehe [`uml/uml.md`](uml/uml.md)): MP3-Stream über HTTP laden und dekodieren. Braucht zusätzlich `https://github.com/pschatzmann/arduino-libhelix` in `lib_deps`.

```cpp
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"

URLStream url;                                   // HTTP-Quelle (WLAN muss verbunden sein)
MP3DecoderHelix mp3;
EncodedAudioStream decoded(&kit, &mp3);          // dekodiert und schreibt in kit
StreamCopy copier(decoded, url);

void setup() {
    decoded.begin();
    url.begin("http://stream.example.com/radio.mp3", "audio/mp3");
}
void loop() { copier.copy(); }
```

`ICYStream` statt `URLStream` liefert zusätzlich den Titel des Radiosenders.

---

## 3. Arduino Audio Driver (Codec ES8388)

Steuert den Codec-Chip über I2C (Lautstärke, Stummschaltung, Ein-/Ausgänge). Meist nutzt man ihn **indirekt** über `AudioBoardStream` (Abschnitt 2). Direkt braucht man ihn nur für Sonderfälle.

### Board-Objekte

Für das AI-Thinker Audio Kit gibt es zwei Varianten mit unterschiedlichen Pins. Welche passt, hängt von der Platinenrevision ab: einfach beide testen, die richtige startet ohne Fehlermeldung und spielt Ton.

| Objekt | I2C (SCL, SDA) | I2S (MCLK, BCK, WS, DOUT, DIN) |
|--------|----------------|--------------------------------|
| `AudioKitEs8388V1` | 32, 33 | 0, 27, 25, 26, 35 |
| `AudioKitEs8388V2` | 23, 18 | 0, 5, 25, 26, 35 |

Gemeinsam: Verstärker (PA) GPIO 21, LED GPIO 22, Kopfhörer-Erkennung GPIO 39, Tasten 36, 13, 19, 23, 18, 5.

> Diese Pins gehören in `lib/config/board_pins.h`, falls ihr sie irgendwo selbst braucht.

### Direkt verwenden

```cpp
#include "AudioBoard.h"

CodecConfig codec_cfg;
codec_cfg.output_device = DAC_OUTPUT_ALL;
codec_cfg.i2s.bits = BIT_LENGTH_16BITS;
codec_cfg.i2s.rate = RATE_44K;

AudioKitEs8388V1.begin(codec_cfg);
```

| Funktion | Beschreibung | Beispiel |
|----------|--------------|----------|
| `begin(cfg)` | Codec per I2C einrichten | `AudioKitEs8388V1.begin(codec_cfg);` |
| `end()` | Codec abschalten | `AudioKitEs8388V1.end();` |
| `setVolume(int)` | Lautstärke **0–100** | `AudioKitEs8388V1.setVolume(70);` |
| `getVolume()` | aktuelle Lautstärke 0–100 | `int v = AudioKitEs8388V1.getVolume();` |
| `setMute(bool)` | stumm | `AudioKitEs8388V1.setMute(true);` |
| `setPAPower(bool)` | Verstärker an/aus | `AudioKitEs8388V1.setPAPower(false);` |
| `setConfig(cfg)` | Konfiguration ändern (z. B. Abtastrate) | `AudioKitEs8388V1.setConfig(codec_cfg);` |
| `isKeyPressed(n)` | Taste n gedrückt? | `if (AudioKitEs8388V1.isKeyPressed(1)) ...` |

Werte für `codec_cfg.i2s.rate`: `RATE_8K`, `RATE_16K`, `RATE_22K`, `RATE_32K`, `RATE_44K`, `RATE_48K`.

### Logging

```cpp
AudioDriverLogger.begin(Serial, AudioDriverLogLevel::Warning);
```

---

## 4. ESP32 WiFi

Arduino-WiFi-Bibliothek des ESP32-Cores (Version 3.x). Für `wifi_manager`.

```cpp
#include <WiFi.h>
```

### `WiFi.mode(mode)`

Betriebsart festlegen: `WIFI_STA` (mit Router verbinden), `WIFI_AP` (eigenes Netz), `WIFI_OFF` (Funk aus).

```cpp
WiFi.mode(WIFI_STA);
WiFi.mode(WIFI_OFF);   // für wifi_manager_stop()
```

### `WiFi.setAutoReconnect(bool)`

Automatisches Wiederverbinden des Cores. Laut [`interfaces.md`](interfaces.md) entscheidet der `mode_controller`, deshalb **ausschalten**.

```cpp
WiFi.setAutoReconnect(false);
```

### `WiFi.persistent(bool)`

Bei `false` speichert der ESP32 die Zugangsdaten nicht im Flash (sie kommen ja aus `secrets.h`). Schont den Flash.

```cpp
WiFi.persistent(false);
```

### `WiFi.setHostname(name)`

Name im Router. Vor `WiFi.mode()` / `WiFi.begin()` aufrufen.

```cpp
WiFi.setHostname("esp32-audio");
```

### `WiFi.begin(ssid, password)`

Startet den Verbindungsversuch und kehrt **sofort** zurück. Ob es geklappt hat, zeigen `WiFi.status()` oder die Events.

```cpp
#include "secrets.h"
WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
```

### `WiFi.status()`

| Wert | Bedeutung |
|------|-----------|
| `WL_CONNECTED` | verbunden, IP vorhanden |
| `WL_IDLE_STATUS` | versucht gerade |
| `WL_NO_SSID_AVAIL` | Netz nicht gefunden |
| `WL_CONNECT_FAILED` | z. B. falsches Passwort |
| `WL_CONNECTION_LOST` | Verbindung verloren |
| `WL_DISCONNECTED` | nicht verbunden |

```cpp
if (WiFi.status() == WL_CONNECTED) { /* ... */ }
```

### `WiFi.isConnected()`

Kurzform für `status() == WL_CONNECTED`.

```cpp
bool ok = WiFi.isConnected();
```

### Timeout ohne Blockieren

`WiFi.waitForConnectResult()` blockiert. Für `wifi_manager_start()` („kehrt sofort zurück“) stattdessen die Zeit mit `millis()` prüfen:

```cpp
uint32_t start_ms = millis();
WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

// später, z. B. in einer update()-Funktion:
if (!WiFi.isConnected() && millis() - start_ms > 15000) {
    // Timeout
}
```

### `WiFi.onEvent(callback, event)`

Ruft eine Funktion auf, wenn ein WLAN-Ereignis passiert. Der Callback läuft in einem System-Task, also nur kurz arbeiten. Rückgabe ist eine ID zum späteren Entfernen.

| Event | Bedeutung |
|-------|-----------|
| `ARDUINO_EVENT_WIFI_STA_START` | Station-Modus gestartet |
| `ARDUINO_EVENT_WIFI_STA_CONNECTED` | mit Router verbunden (noch keine IP) |
| `ARDUINO_EVENT_WIFI_STA_GOT_IP` | IP erhalten, ab jetzt nutzbar |
| `ARDUINO_EVENT_WIFI_STA_DISCONNECTED` | getrennt oder Verbindungsversuch gescheitert |
| `ARDUINO_EVENT_WIFI_STA_STOP` | Station-Modus beendet |

```cpp
void on_wifi(arduino_event_id_t event, arduino_event_info_t info) {
    switch (event) {
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
            break;
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            Serial.printf("getrennt, Grund %d\n", info.wifi_sta_disconnected.reason);
            break;
        default: break;
    }
}

wifi_event_id_t id = WiFi.onEvent(on_wifi);                               // alle Events
WiFi.onEvent(on_wifi, ARDUINO_EVENT_WIFI_STA_GOT_IP);                     // nur eines
```

Häufige Gründe bei `DISCONNECTED`: 201 = Netz nicht gefunden, 15/204 = falsches Passwort (Handshake-Timeout), 8 = selbst getrennt.

### `WiFi.removeEvent(id)`

Event-Callback wieder abmelden.

```cpp
WiFi.removeEvent(id);
```

### `WiFi.disconnect(wifioff, eraseap)`

Trennt die Verbindung. Mit `wifioff = true` wird zusätzlich der Funk abgeschaltet.

```cpp
WiFi.disconnect(true);   // trennen und Funk aus
WiFi.mode(WIFI_OFF);     // sicherheitshalber Modus aus
```

### Infos abfragen

| Funktion | Beispielausgabe |
|----------|-----------------|
| `WiFi.localIP()` | `192.168.1.42` |
| `WiFi.gatewayIP()` | `192.168.1.1` |
| `WiFi.dnsIP()` | `192.168.1.1` |
| `WiFi.RSSI()` | `-58` (dBm; besser als -70 ist gut) |
| `WiFi.SSID()` | Name des verbundenen Netzes |
| `WiFi.macAddress()` | `24:6F:28:...` |

```cpp
Serial.printf("IP %s, RSSI %d dBm\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
```

### `WiFi.setSleep(bool)`

Modem-Sleep des WLAN. **Muss an bleiben (`true`, Standard), solange Bluetooth aktiv ist**, sonst stürzt der ESP32 ab („Should enable WiFi modem sleep when both WiFi and Bluetooth are enabled“). Also nicht anfassen.

```cpp
WiFi.setSleep(true);   // Standard, mit Bluetooth zwingend
```

### `WiFi.scanNetworks()`

Sucht Netze in der Umgebung. Zum Debuggen, wenn die Verbindung nicht klappt.

```cpp
int n = WiFi.scanNetworks();
for (int i = 0; i < n; i++) {
    Serial.printf("%s (%d dBm)\n", WiFi.SSID(i).c_str(), WiFi.RSSI(i));
}
WiFi.scanDelete();
```

### mDNS

Macht den ESP32 als `esp32-audio.local` im Netz auffindbar. Spotify Connect (cspot) nutzt mDNS, um in der App aufzutauchen; cspot richtet das in der Regel selbst ein.

```cpp
#include <ESPmDNS.h>
MDNS.begin("esp32-audio");
MDNS.end();
```

---

## 5. HTTPS und JSON (Grundlage für Spotify)

Die Spotify Web API läuft nur über HTTPS. Dafür braucht man `WiFiClientSecure` (TLS), `HTTPClient` (Anfragen) und `ArduinoJson` (Antworten lesen).

```cpp
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <base64.h>
```

### `WiFiClientSecure`

TLS-Verbindung. Entweder das Zertifikat prüfen (`setCACert`) oder zum Testen darauf verzichten (`setInsecure`).

```cpp
WiFiClientSecure client;
client.setInsecure();            // nur für Tests: Zertifikat wird nicht geprüft
// client.setCACert(root_ca);    // sicher: Root-Zertifikat als PEM-String
client.setTimeout(10);           // Sekunden
```

Eine TLS-Verbindung braucht ca. 40–50 KB Heap. Mit Bluetooth und cspot gleichzeitig wird das knapp: `ESP.getFreeHeap()` beobachten.

### `HTTPClient`

| Funktion | Beschreibung |
|----------|--------------|
| `begin(client, url)` | Anfrage vorbereiten |
| `addHeader(name, value)` | Header setzen (vor dem Senden) |
| `setTimeout(ms)` | Timeout in ms |
| `GET()` | GET senden, Rückgabe HTTP-Code |
| `POST(body)` / `PUT(body)` | mit Body senden |
| `sendRequest("PUT", body)` | beliebige Methode, auch `"DELETE"` |
| `getString()` | Antwort als String |
| `getStream()` | Antwort als Stream (spart RAM bei großen Antworten) |
| `getSize()` | Länge der Antwort, -1 wenn unbekannt |
| `collectHeaders(keys, n)` | Antwort-Header vormerken |
| `header(name)` | vorgemerkten Antwort-Header lesen |
| `end()` | Verbindung schließen, **immer aufrufen** |

Rückgabewerte unter 0 sind Verbindungsfehler (z. B. `-1` = Verbindung fehlgeschlagen); `http.errorToString(code)` liefert den Text.

```cpp
HTTPClient http;
http.begin(client, "https://api.spotify.com/v1/me/player");
http.addHeader("Authorization", "Bearer " + access_token);
int code = http.GET();
if (code == 200) {
    String body = http.getString();
} else if (code < 0) {
    Serial.println(http.errorToString(code));
}
http.end();
```

Header einer Antwort lesen (z. B. `Retry-After` bei Rate-Limit):

```cpp
const char *keys[] = {"Retry-After"};
http.collectHeaders(keys, 1);     // vor GET()
int code = http.GET();
if (code == 429) {
    int wait_s = http.header("Retry-After").toInt();
}
```

### `base64::encode()`

Für den `Authorization: Basic ...`-Header beim Token-Abruf.

```cpp
String basic = base64::encode(String(CLIENT_ID) + ":" + CLIENT_SECRET);
http.addHeader("Authorization", "Basic " + basic);
```

### ArduinoJson 7: Antwort lesen

`JsonDocument` passt seine Größe selbst an. `deserializeJson()` liest direkt aus String oder Stream.

```cpp
JsonDocument doc;
DeserializationError err = deserializeJson(doc, http.getStream());
if (err) {
    Serial.println(err.c_str());
}
bool playing   = doc["is_playing"];                    // bool
const char *t  = doc["item"]["name"];                  // String-Feld
int volume     = doc["device"]["volume_percent"] | 0;  // mit Standardwert
```

### Filter: nur benötigte Felder

Spotify-Antworten sind groß (mehrere KB). Ein Filter lässt nur die gewünschten Felder in den RAM.

```cpp
JsonDocument filter;
filter["is_playing"] = true;
filter["item"]["name"] = true;
filter["device"]["name"] = true;

JsonDocument doc;
deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
```

### Arrays durchlaufen

```cpp
for (JsonObject dev : doc["devices"].as<JsonArray>()) {
    Serial.println(dev["name"].as<const char *>());
}
```

### JSON erzeugen (Body für PUT/POST)

```cpp
JsonDocument body;
body["device_ids"][0] = device_id;
body["play"] = true;
String out;
serializeJson(body, out);   // {"device_ids":["..."],"play":true}
```

---

## 6. Spotify Web API

**Wichtig:** Die Web API liefert **keine Audiodaten**. Sie ist nur Fernbedienung und Infoquelle (was läuft, welches Gerät, Lautstärke). Die Musik kommt über Spotify Connect (cspot). Die Web API ist nützlich, um z. B. den Adapter per Code als Wiedergabegerät auszuwählen, Metadaten anzuzeigen oder zu testen. Alle Steuerbefehle funktionieren nur mit **Spotify Premium**.

Offizielle Doku: <https://developer.spotify.com/documentation/web-api>

### Einmalige Einrichtung

1. Auf <https://developer.spotify.com/dashboard> eine App anlegen.
2. **Redirect URI** eintragen: `http://127.0.0.1:8888/callback`. Spotify erlaubt nur HTTPS oder Loopback-Adressen (`127.0.0.1`), nicht `localhost` und keine lokale IP des ESP32.
3. **Client ID** und **Client Secret** notieren und in `secrets.h` eintragen (nie committen).
4. Unter *User Management* das eigene Spotify-Konto eintragen (Apps im Development Mode funktionieren nur für eingetragene Nutzer).

### Scopes (Berechtigungen)

| Scope | Erlaubt |
|-------|---------|
| `user-read-playback-state` | Player-Status und Geräte lesen |
| `user-modify-playback-state` | Play, Pause, Skip, Lautstärke, Gerät wechseln |
| `user-read-currently-playing` | aktuell laufenden Titel lesen |

### Ablauf der Anmeldung (Authorization Code Flow)

Der ESP32 kann sich nicht selbst im Browser anmelden. Deshalb einmal am PC einen **Refresh Token** holen, in `secrets.h` speichern, und der ESP32 tauscht ihn bei Bedarf gegen kurzlebige Access Tokens (1 h gültig).

**Schritt 1: Im Browser öffnen** (eine Zeile):

```
https://accounts.spotify.com/authorize?client_id=DEINE_CLIENT_ID&response_type=code&redirect_uri=http%3A%2F%2F127.0.0.1%3A8888%2Fcallback&scope=user-read-playback-state%20user-modify-playback-state%20user-read-currently-playing
```

Nach dem Login landet der Browser auf `http://127.0.0.1:8888/callback?code=AQB...`. Die Seite lädt nicht, das ist egal: den Wert hinter `code=` kopieren (gilt nur wenige Minuten).

**Schritt 2: Code gegen Tokens tauschen** (am PC, Git Bash):

```bash
curl -X POST https://accounts.spotify.com/api/token \
  -u "DEINE_CLIENT_ID:DEIN_CLIENT_SECRET" \
  -d grant_type=authorization_code \
  -d code=AQB... \
  -d redirect_uri=http://127.0.0.1:8888/callback
```

Antwort:

```json
{
  "access_token": "BQD...",
  "token_type": "Bearer",
  "expires_in": 3600,
  "refresh_token": "AQC...",
  "scope": "user-read-playback-state user-modify-playback-state ..."
}
```

`refresh_token` in `secrets.h` als `SPOTIFY_REFRESH_TOKEN` eintragen.

### Access Token erneuern (auf dem ESP32)

`POST https://accounts.spotify.com/api/token`. Das gibt einen neuen Access Token für 1 Stunde. Vor Ablauf (oder bei Antwort 401) erneut aufrufen.

```cpp
HTTPClient http;
http.begin(client, "https://accounts.spotify.com/api/token");
http.addHeader("Content-Type", "application/x-www-form-urlencoded");
http.addHeader("Authorization", "Basic " + base64::encode(String(SPOTIFY_CLIENT_ID) + ":" + SPOTIFY_CLIENT_SECRET));
int code = http.POST(String("grant_type=refresh_token&refresh_token=") + SPOTIFY_REFRESH_TOKEN);

if (code == 200) {
    JsonDocument doc;
    deserializeJson(doc, http.getStream());
    String access_token = doc["access_token"].as<String>();
    uint32_t expires_ms = millis() + (doc["expires_in"].as<uint32_t>() - 60) * 1000;
}
http.end();
```

Enthält die Antwort ein neues `refresh_token`, ersetzt es das alte (dann z. B. mit `Preferences` speichern, Abschnitt 7).

### Alle Anfragen an die API

Basis-URL `https://api.spotify.com/v1`, Header immer:

```
Authorization: Bearer <access_token>
```

Bei PUT/POST mit Body zusätzlich `Content-Type: application/json`. Bei PUT/POST **ohne** Body trotzdem einen leeren Body senden (`http.PUT("")`), sonst antwortet Spotify manchmal mit 411.

### Statuscodes

| Code | Bedeutung | Reaktion |
|------|-----------|----------|
| 200 | OK, mit JSON | auswerten |
| 202 / 204 | OK, ohne Inhalt (bei `GET /me/player`: nichts aktiv) | nichts tun |
| 400 | falsche Anfrage | URL/Body prüfen |
| 401 | Access Token abgelaufen/ungültig | Token erneuern, erneut versuchen |
| 403 | verboten (kein Premium, fehlender Scope) | Scopes prüfen |
| 404 | kein aktives Gerät | Gerät auswählen (`PUT /me/player`) |
| 429 | zu viele Anfragen | `Retry-After` Sekunden warten |
| 5xx | Spotify-Fehler | später erneut |

### Endpunkte

#### `GET /me` – eigenes Profil

Testet, ob der Token funktioniert. Feld `product` zeigt `"premium"` oder `"free"`.

```cpp
http.begin(client, "https://api.spotify.com/v1/me");
http.addHeader("Authorization", "Bearer " + access_token);
int code = http.GET();   // 200: {"display_name":"...","product":"premium",...}
```

#### `GET /me/player` – Player-Status

Aktives Gerät, läuft etwas, Titel, Lautstärke. **204** = gerade kein Gerät aktiv.

```cpp
http.begin(client, "https://api.spotify.com/v1/me/player");
http.addHeader("Authorization", "Bearer " + access_token);
int code = http.GET();
```

Wichtige Felder der Antwort:

| Feld | Inhalt |
|------|--------|
| `is_playing` | `true` / `false` |
| `device.id`, `device.name` | aktives Gerät, z. B. `"ESP32 Audio Adapter"` |
| `device.volume_percent` | 0–100 |
| `item.name` | Titel |
| `item.artists[0].name` | Interpret |
| `item.duration_ms`, `progress_ms` | Länge und Position |
| `shuffle_state`, `repeat_state` | Zufall, Wiederholung |

#### `GET /me/player/currently-playing` – aktueller Titel

Wie `/me/player`, aber nur Titel und Position. 204 = nichts läuft.

```cpp
http.begin(client, "https://api.spotify.com/v1/me/player/currently-playing");
```

#### `GET /me/player/devices` – verfügbare Geräte

Listet alle Spotify-Connect-Geräte. Hier taucht der Adapter auf, sobald cspot läuft; `id` braucht man für die Befehle unten.

```cpp
http.begin(client, "https://api.spotify.com/v1/me/player/devices");
// Antwort: {"devices":[{"id":"abc123","name":"ESP32 Audio Adapter","is_active":false,"volume_percent":50,"type":"Speaker"}]}
```

#### `PUT /me/player` – Wiedergabe auf ein Gerät übertragen

Macht ein Gerät aktiv. Mit `"play": true` startet die Wiedergabe dort sofort. Antwort 204.

```cpp
http.begin(client, "https://api.spotify.com/v1/me/player");
http.addHeader("Authorization", "Bearer " + access_token);
http.addHeader("Content-Type", "application/json");
int code = http.PUT("{\"device_ids\":[\"abc123\"],\"play\":true}");
```

#### `PUT /me/player/play` – Wiedergabe starten oder fortsetzen

Ohne Body: fortsetzen. Mit Body: bestimmte Playlist/Album/Titel starten. Optional `?device_id=` für ein bestimmtes Gerät.

```cpp
// fortsetzen
http.begin(client, "https://api.spotify.com/v1/me/player/play");
http.PUT("");

// Playlist auf dem Adapter starten
http.begin(client, "https://api.spotify.com/v1/me/player/play?device_id=abc123");
http.addHeader("Content-Type", "application/json");
http.PUT("{\"context_uri\":\"spotify:playlist:37i9dQZF1DXcBWIGoYBM5M\"}");

// einzelne Titel
http.PUT("{\"uris\":[\"spotify:track:4uLU6hMCjMI75M1A2tKUQC\"]}");
```

Weitere Body-Felder: `"offset":{"position":3}` (ab dem 4. Titel), `"position_ms":30000` (ab 30 s).

#### `PUT /me/player/pause` – Pause

```cpp
http.begin(client, "https://api.spotify.com/v1/me/player/pause");
http.PUT("");
```

#### `POST /me/player/next` und `POST /me/player/previous` – Titel überspringen

```cpp
http.begin(client, "https://api.spotify.com/v1/me/player/next");
http.POST("");
http.begin(client, "https://api.spotify.com/v1/me/player/previous");
http.POST("");
```

#### `PUT /me/player/volume?volume_percent=X` – Lautstärke

0–100. Wirkt auf das aktive Gerät (bei cspot kommt der Wert dann als 0–65535 an).

```cpp
http.begin(client, "https://api.spotify.com/v1/me/player/volume?volume_percent=40");
http.PUT("");
```

#### `PUT /me/player/seek?position_ms=X` – Position im Titel

```cpp
http.begin(client, "https://api.spotify.com/v1/me/player/seek?position_ms=60000");
http.PUT("");
```

#### `PUT /me/player/shuffle?state=true|false` – Zufallswiedergabe

```cpp
http.begin(client, "https://api.spotify.com/v1/me/player/shuffle?state=true");
http.PUT("");
```

#### `PUT /me/player/repeat?state=track|context|off` – Wiederholen

```cpp
http.begin(client, "https://api.spotify.com/v1/me/player/repeat?state=context");
http.PUT("");
```

#### `POST /me/player/queue?uri=X` – Titel in die Warteschlange

URI muss URL-kodiert sein (`:` → `%3A`).

```cpp
http.begin(client, "https://api.spotify.com/v1/me/player/queue?uri=spotify%3Atrack%3A4uLU6hMCjMI75M1A2tKUQC");
http.POST("");
```

#### Endpunkte am PC testen

Spart Flash-Zyklen: erst mit `curl` ausprobieren, dann in C++ umsetzen.

```bash
curl -H "Authorization: Bearer BQD..." https://api.spotify.com/v1/me/player/devices
curl -X PUT -H "Authorization: Bearer BQD..." -H "Content-Length: 0" https://api.spotify.com/v1/me/player/pause
```

### Hinweise

- Nicht öfter als ca. alle 2–5 s pollen, sonst kommt 429.
- Für `spotify_connect` (Ereignisse `EVT_SPOTIFY_PLAYBACK_STARTED/STOPPED`) sind die Callbacks von **cspot** die richtige Quelle, nicht das Pollen der Web API. cspot ist hier nicht beschrieben, weil die Einbindung erst im Machbarkeitstest geklärt wird.

---

## 7. ESP32, FreeRTOS und Hilfsmittel

### FreeRTOS-Queue (für `mode_controller_post`)

Thread-sichere Warteschlange. Ideal für Ereignisse: Callbacks aus beliebigen Tasks legen ein Ereignis hinein, `loop()` holt es heraus.

```cpp
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

QueueHandle_t q = xQueueCreate(16, sizeof(app_event_t));   // 16 Einträge

// senden, ohne zu warten (Timeout 0) → blockiert nie
app_event_t e = EVT_BT_CONNECTED;
if (xQueueSend(q, &e, 0) != pdTRUE) {
    // Queue voll
}

// empfangen, ohne zu warten
app_event_t got;
while (xQueueReceive(q, &got, 0) == pdTRUE) {
    // got verarbeiten
}
```

Aus einer Interrupt-Routine (z. B. Tasten-ISR) stattdessen:

```cpp
BaseType_t woken = pdFALSE;
xQueueSendFromISR(q, &e, &woken);
portYIELD_FROM_ISR(woken);
```

Weitere: `uxQueueMessagesWaiting(q)` (Anzahl Einträge), `xQueueReset(q)` (leeren).

### Variablen zwischen Tasks teilen

Für einfache Werte (z. B. Lautstärke) reicht `std::atomic`:

```cpp
#include <atomic>
std::atomic<uint8_t> volume_percent{50};
volume_percent.store(70);
uint8_t v = volume_percent.load();
```

Für mehrere Werte zusammen eine kritische Sektion oder einen Mutex:

```cpp
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
portENTER_CRITICAL(&mux);
// kurz Daten ändern, kein delay, kein Serial
portEXIT_CRITICAL(&mux);
```

```cpp
SemaphoreHandle_t m = xSemaphoreCreateMutex();
if (xSemaphoreTake(m, pdMS_TO_TICKS(50)) == pdTRUE) {
    // geschützter Bereich
    xSemaphoreGive(m);
}
```

### Eigener Task

Für Arbeit, die dauerhaft im Hintergrund läuft (z. B. Spotify-Polling), damit `loop()` frei bleibt.

```cpp
void poll_task(void *arg) {
    for (;;) {
        // arbeiten
        vTaskDelay(pdMS_TO_TICKS(3000));   // 3 s schlafen, gibt CPU frei
    }
}

TaskHandle_t h;
xTaskCreatePinnedToCore(poll_task, "poll", 8192, nullptr, 2, &h, 0);
// Name, Stack in Bytes, Parameter, Priorität, Handle, Kern
vTaskDelete(h);   // Task beenden
```

### Timer mit `millis()` (z. B. Retry nach 60 s)

`millis()` läuft nach ca. 49 Tagen über. Die Differenz `millis() - start` ist trotzdem korrekt, solange beide `uint32_t` sind. Nie `millis() > start + 60000` schreiben.

```cpp
uint32_t retry_start = 0;
bool retry_active = false;

void start_retry() { retry_start = millis(); retry_active = true; }

// in update():
if (retry_active && millis() - retry_start >= 60000) {
    retry_active = false;
    // EVT_RETRY_TIMER auslösen
}
```

### `Ticker` (periodischer Aufruf)

Ruft eine Funktion in festen Abständen auf. Läuft im Timer-Task, also nur kurz arbeiten (z. B. Ereignis in die Queue legen).

```cpp
#include <Ticker.h>
Ticker retry_ticker;
retry_ticker.once(60, on_retry);       // einmal nach 60 s
retry_ticker.attach(1.0, on_tick);     // jede Sekunde
retry_ticker.detach();                 // stoppen
```

### Speicher und System

| Funktion | Beschreibung |
|----------|--------------|
| `ESP.getFreeHeap()` | freier Heap in Bytes |
| `ESP.getMinFreeHeap()` | kleinster freier Heap seit Start (zeigt Engpässe) |
| `ESP.getMaxAllocHeap()` | größter zusammenhängender Block |
| `psramFound()` | `true`, wenn PSRAM aktiv |
| `ESP.getFreePsram()` | freier PSRAM |
| `ESP.restart()` | Neustart (für den ERROR-Zustand) |
| `uxTaskGetStackHighWaterMark(nullptr)` | wie viel Stack des aktuellen Tasks nie benutzt wurde |

```cpp
Serial.printf("Heap %u, min %u, PSRAM %u\n",
              ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getFreePsram());
```

### Logging

Die ESP-Logs berücksichtigen `CORE_DEBUG_LEVEL` aus der `platformio.ini` (1 = Error … 5 = Verbose). Ausgabe mit Datei, Zeile und Stufe.

```cpp
log_e("Codec-Start fehlgeschlagen");
log_w("WLAN-Timeout nach %d ms", 15000);
log_i("Modus: %d", mode);
log_d("nur sichtbar bei Level 4+");
```

### Serielle Eingabe (Ereignisse von Hand auslösen)

Zum Testen des Zustandsautomaten auf dem Board, wie in [`interfaces.md`](interfaces.md) vorgesehen.

```cpp
if (Serial.available()) {
    char c = Serial.read();
    switch (c) {
        case 'b': /* EVT_BT_CONNECTED */ break;
        case 'w': /* EVT_WIFI_CONNECTED */ break;
    }
}
```

### `Preferences` (Werte dauerhaft speichern)

Speichert Werte im Flash (NVS), z. B. letzte Lautstärke oder einen neuen Refresh Token.

```cpp
#include <Preferences.h>
Preferences prefs;

prefs.begin("audio", false);             // Namensraum, false = lesen und schreiben
prefs.putUChar("volume", 60);
uint8_t vol = prefs.getUChar("volume", 50);   // 50 = Standard
prefs.putString("refresh", token);
String t = prefs.getString("refresh", "");
prefs.end();
```

Nicht bei jeder Lautstärkeänderung schreiben (Flash-Verschleiß): erst speichern, wenn der Wert ein paar Sekunden gleich bleibt.

### GPIO (Taster, LED)

```cpp
pinMode(22, OUTPUT);          // LED auf dem Audio Kit
digitalWrite(22, HIGH);

pinMode(36, INPUT);           // GPIO 34–39: nur Eingang, kein interner Pull-up
bool pressed = digitalRead(36) == LOW;

attachInterrupt(digitalPinToInterrupt(36), on_button, FALLING);
```

---

## 8. Unit-Tests mit Unity

Für `pio test -e native`. Tests liegen in `test/<name>/test_main.cpp`. Am PC gibt es kein `Arduino.h`; deshalb ist `mode_fsm` reine Logik ohne Arduino-Aufrufe.

```cpp
#include <unity.h>
#include "mode_fsm.h"

void setUp(void) {}      // vor jedem Test
void tearDown(void) {}   // nach jedem Test

void test_bt_connect_wins(void) {
    // Zustand herstellen, Ereignis geben, Ergebnis prüfen
    TEST_ASSERT_EQUAL(MODE_BT_STREAMING, /* neuer Zustand */ MODE_BT_STREAMING);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_bt_connect_wins);
    return UNITY_END();
}
```

| Makro | Prüft |
|-------|-------|
| `TEST_ASSERT_TRUE(x)` / `TEST_ASSERT_FALSE(x)` | Bedingung |
| `TEST_ASSERT_EQUAL(erwartet, ist)` | Gleichheit (ganze Zahlen, Enums) |
| `TEST_ASSERT_EQUAL_UINT32(e, i)` | mit fester Breite |
| `TEST_ASSERT_EQUAL_STRING(e, i)` | C-Strings |
| `TEST_ASSERT_EQUAL_MEMORY(e, i, len)` | Byte-Bereiche |
| `TEST_ASSERT_EQUAL_MESSAGE(e, i, "Text")` | mit Meldung bei Fehler |
| `TEST_FAIL_MESSAGE("Text")` | Test bewusst fehlschlagen lassen |

Auf dem Board (statt `native`) heißt der Einstieg `setup()` mit `UNITY_BEGIN() ... UNITY_END()` und leerem `loop()`.

---

## 9. PlatformIO-Befehle

In VS Code im PlatformIO-Terminal oder in jeder Konsole mit `pio` im Pfad.

| Befehl | Wirkung |
|--------|---------|
| `pio run` | bauen |
| `pio run -t upload` | bauen und flashen |
| `pio device monitor` | serielle Konsole (Baudrate aus `monitor_speed`) |
| `pio run -t upload -t monitor` | flashen und direkt Konsole öffnen |
| `pio run -t clean` | Build-Ordner leeren |
| `pio run -t erase` | gesamten Flash löschen (auch gespeicherte BT-Kopplung, Preferences) |
| `pio test -e native` | Unit-Tests am PC |
| `pio test -e esp32dev` | Unit-Tests auf dem Board |
| `pio run -t size` | Speicherbedarf anzeigen |
| `pio pkg update` | Bibliotheken aktualisieren |
| `pio device list` | angeschlossene Boards/COM-Ports |

Nützliche Einträge in `platformio.ini`:

```ini
build_flags =
    -DCORE_DEBUG_LEVEL=2
    -DBOARD_HAS_PSRAM                 ; PSRAM aktivieren (ESP32-A1S hat 4 MB)
    -mfix-esp32-psram-cache-issue
upload_port  = COM5                   ; falls das Board nicht automatisch gefunden wird
upload_speed = 921600
```

Absturz-Backtraces werden durch `monitor_filters = esp32_exception_decoder` automatisch in Dateiname und Zeile übersetzt.

---

## 10. Stolperfallen

| Problem | Ursache und Lösung |
|---------|--------------------|
| Absturz „Should enable WiFi modem sleep…“ | `WiFi.setSleep(false)` bei aktivem Bluetooth. Modem-Sleep anlassen. |
| Kein Ton, `begin()` schlägt fehl | falsches Board-Objekt: `AudioKitEs8388V1` und `V2` ausprobieren. |
| Tasten oder Pins spinnen | SD-Karte aktiv; `cfg.sd_active = false`. |
| Ton wird doppelt leiser | A2DP-Lautstärke und Codec-Lautstärke gleichzeitig; nur an einer Stelle regeln. |
| Knacken beim Quellenwechsel | vor dem Wechsel `setMute(true)`, I2S leeren, danach wieder an. |
| Aussetzer bei Bluetooth | Log-Level zu hoch, `Serial.print` im Audio-Callback, zu kleine I2S-Puffer. |
| Bluetooth startet nach `end(true)` nicht mehr | `end(true)` gibt den BT-Speicher endgültig frei; nur `end()` benutzen. |
| Upload: Programm zu groß | Partition `huge_app.csv` (ist schon eingetragen). |
| Wenig Heap mit BT + WLAN + TLS | PSRAM aktivieren, TLS-Verbindungen sofort mit `http.end()` schließen, `ESP.getMinFreeHeap()` beobachten. |
| Spotify 401 | Access Token abgelaufen (nach 1 h), neu holen. |
| Spotify 404 bei Play/Pause | kein aktives Gerät; erst `PUT /me/player` mit Geräte-ID. |
| Callback blockiert alles | Callbacks (BT, WLAN, Ticker) laufen in fremden Tasks: nur Ereignis in die Queue legen, Arbeit in `loop()`. |
| Header verweisen auf `docs/schnittstellen.md` | Die Datei heißt `docs/interfaces.md`. |
