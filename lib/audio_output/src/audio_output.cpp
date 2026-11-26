#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"   // braucht arduino-audio-driver
#include "audio_output.h"

// Genau EIN Board-Objekt für das ganze Programm. Als lokale Variable in
// audio_output_init() würde es am Ende der Funktion wieder zerstört.
static AudioBoardStream kit(AudioKitEs8388V1);   // kein Ton? AudioKitEs8388V2 probieren

bool audio_output_init(void) {
    auto cfg = kit.defaultConfig(TX_MODE);
    cfg.sample_rate     = AUDIO_OUTPUT_DEFAULT_RATE_HZ;
    cfg.channels        = 2;
    cfg.bits_per_sample = 16;
    cfg.sd_active       = false;   // SD-Karte aus, sonst belegt sie Pins (z. B. GPIO 13)
    cfg.buffer_count    = 8;       // Anzahl DMA-Puffer
    cfg.buffer_size     = 512;     // Bytes pro DMA-Puffer

    if (!kit.begin(cfg)) {
        Serial.println("Audio output initialization failed.");
        return false;
    }
    Serial.printf("Audio output initialized, %u Hz.\n", (unsigned)cfg.sample_rate);
    return true;
}

// TODO Schritt 1: audio_output_set_sample_rate(), audio_output_write(),
// audio_output_set_volume(), audio_output_get_volume(), audio_output_stop()
