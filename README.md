# ESP32 Audio Data Receiver

Ein kleiner Audio-Adapter auf Basis des ESP32, der analoge Kopfhörer und Lautsprecher kabellos macht. Musik kommt entweder über das Heim-WLAN direkt von Spotify oder per Bluetooth vom Smartphone.

Software-Engineering-Projekt im Bachelor Elektrotechnik (Semester 6).

> **Status:** in Entwicklung

---

## Funktionen

- **WLAN-Betrieb (Standard):** Nach dem Einschalten verbindet sich der Adapter mit dem Heim-WLAN und erscheint in der Spotify-App als Wiedergabegerät (Spotify Connect). Der ESP32 lädt die Audiodaten selbst; das Smartphone dient nur als Fernbedienung.
- **Bluetooth-Betrieb:** Der Adapter ist jederzeit als Bluetooth-Lautsprecher (A2DP) sichtbar. Verbindet sich ein Smartphone, hat Bluetooth Vorrang: WLAN wird abgeschaltet, bis die Verbindung getrennt wird.
- **Ohne WLAN nutzbar:** Findet der Adapter kein WLAN, wartet er im Leerlauf, Bluetooth funktioniert weiter, und er versucht es regelmäßig erneut.

| Modus | Audioquelle | WLAN | Bluetooth sichtbar |
|-------|-------------|------|--------------------|
| WLAN-Betrieb | Spotify (Spotify Connect) | an | ja |
| Leerlauf (IDLE) | keine | Verbindungsversuch alle 60 s | ja |
| Bluetooth-Betrieb | Smartphone (A2DP) | aus | nein, verbunden |

Details zu Use Cases, Zustandsautomat und Arbeitsteilung stehen in [`docs/uml/uml.md`](docs/uml/uml.md).

## Hardware

- AI-Thinker ESP32 Audio Kit (Modul ESP32-A1S, Codec ES8388)
- Kopfhörer oder Lautsprecher mit Verstärker am Audioausgang
- USB-Kabel zum Flashen und für die Stromversorgung

## Voraussetzungen

- [VS Code](https://code.visualstudio.com/) mit der Erweiterung [PlatformIO IDE](https://platformio.org/install/ide?install=vscode)
- Git
- Für den WLAN-Betrieb: ein Spotify-Premium-Konto (Spotify Connect funktioniert nur mit Premium)

## Schnellstart

```bash
git clone https://github.com/bela-mnnng/ESP32_Audio_Data_Receiver.git
cd ESP32_Audio_Data_Receiver
```

1. Den Ordner in VS Code öffnen. PlatformIO erkennt das Projekt an der `platformio.ini`.
2. Zugangsdaten anlegen: `lib/config/secrets.example.h` nach `lib/config/secrets.h` kopieren und WLAN-Name und Passwort eintragen. Die Datei `secrets.h` wird nicht ins Repo übernommen.
3. Das Audio Kit per USB anschließen.
4. Bauen, flashen und seriellen Monitor öffnen:

```bash
pio run                      # bauen
pio run --target upload      # flashen
pio device monitor           # serielle Ausgabe (115200 Baud)
```

In VS Code geht das auch über die PlatformIO-Leiste unten: Häkchen = Build, Pfeil = Upload, Stecker = Monitor.

## Projektstruktur

```
ESP32_Audio_Data_Receiver/
├── docs/               Anforderungen und UML
├── include/            (leer)
├── lib/                eine Bibliothek pro Komponente
│   ├── config/         Pins und Zugangsdaten
│   ├── audio_output/   I2S und Codec, Lautstärke
│   ├── bt_sink/        Bluetooth-A2DP-Empfänger
│   ├── mode_fsm/       Zustandsautomat (reine Logik, am PC testbar)
│   ├── mode_controller/ verbindet den Automaten mit den Komponenten
│   ├── wifi_manager/   WLAN verbinden, Timeout, Neuversuch
│   └── spotify_connect/ Spotify-Connect-Empfänger
├── src/main.cpp        setup() und loop(), nur Verdrahtung
├── test/               Unit-Tests (Unity)
└── platformio.ini
```

Jede Komponente in `lib/` hat einen öffentlichen Header in `include/` und ihren Code in `src/`. Andere Komponenten binden nur den Header ein.

## Tests

```bash
pio test -e native
```

Die Unit-Tests laufen am PC und prüfen die Logik ohne Hardware, vor allem den Zustandsautomaten. Unter Windows braucht `native` einen installierten GCC-Compiler (z. B. über MSYS2); in GitHub Actions laufen die Tests automatisch.

## Zusammenarbeit

- Für jede Aufgabe ein GitHub-Issue.
- Gearbeitet wird auf Branches `feature/<komponente>-<thema>`, gemergt nur per Pull Request mit Review durch die andere Person.
- Zuständigkeiten pro Ordner stehen in `.github/CODEOWNERS`.
- Änderungen an Headern der anderen Person nur nach Absprache im Issue.

## Team

| Name | Schwerpunkt |
|------|-------------|
|bela-mnnng | Audioausgabe, Bluetooth, Zustandsautomat |
| timaxion | WLAN, Spotify Connect |

## Verwendete Bibliotheken

- [ESP32-A2DP](https://github.com/pschatzmann/ESP32-A2DP) – Bluetooth-A2DP-Sink
- [arduino-audio-tools](https://github.com/pschatzmann/arduino-audio-tools) und [arduino-audio-driver](https://github.com/pschatzmann/arduino-audio-driver) – Audioausgabe und Codec-Treiber
- [cspot](https://github.com/feelfreelinux/cspot) – Spotify Connect (Einbindung wird im Machbarkeitstest geprüft)
