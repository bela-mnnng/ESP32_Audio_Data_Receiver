#include "audio_output.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"   // braucht arduino-audio-driver

bool audio_output_init(void) {
    
    // Implement initialization of AudioTools and I2S here
    AudioInfo info(44100, 2, 16);
    AudioBoardStream kit(AudioKitEs8388V1);   // or AudioKitEs8388V2
    Serial.print("Audio output sample rate: ");
    Serial.println(info.sample_rate);

    auto cfg = kit.defaultConfig(TX_MODE);
    cfg.sample_rate     = 44100;
    cfg.channels        = 2;
    cfg.bits_per_sample = 16;
    cfg.sd_active       = false;   // SD-Karte aus, sonst belegt sie Pins (z. B. GPIO 13)
    cfg.buffer_count    = 8;       // Anzahl DMA-Puffer
    cfg.buffer_size     = 512;     // Bytes pro DMA-Puffer

    kit.begin(cfg);
    Serial.println("Audio output initialized.");

    if(!kit.begin(cfg)) {
        Serial.println("Audio output initialization failed.");
        return false;
    }
    return true; // Return true if initialization is successful
}