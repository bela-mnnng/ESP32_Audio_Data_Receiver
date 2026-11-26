#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"   // braucht arduino-audio-driver
#include "AudioTools/Concurrency/RTOS.h"             // MutexRTOS, LockGuard
#include "audio_output.h"

// Genau EIN Board-Objekt für das ganze Programm. Als lokale Variable in
// audio_output_init() würde es am Ende der Funktion wieder zerstört.
static AudioBoardStream kit(AudioKitEs8388V1);   // kein Ton? AudioKitEs8388V2 probieren

// Schützt die Codec-Befehle (I2C), weil Lautstärke & Co. aus verschiedenen
// Tasks kommen können (loop(), Bluetooth-Task, Spotify-Task).
static MutexRTOS s_codec_mutex;

static uint32_t s_sample_rate    = AUDIO_OUTPUT_DEFAULT_RATE_HZ;
static uint8_t  s_volume_percent = 0;
static volatile bool s_muted     = false;   // gesetzt von audio_output_stop()

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
        kit.end();
        return false;
    }

    // Lautsprecher-Verstärker (Schraubklemmen) aus: Ton nur an der Kopfhörerbuchse.
    // Für Lautsprecher direkt am Board auf true setzen.
    kit.setPAPower(false);
    audio_output_set_volume(50);

    Serial.printf("Audio output initialized, %u Hz, volume %u %%.\n",
                  (unsigned)s_sample_rate, (unsigned)s_volume_percent);
    return true;
}

bool audio_output_set_sample_rate(uint32_t rate_hz) {
    // Laut Header nur 44100 oder 48000. Nicht "zurechtbiegen": Mit falscher Rate
    // würde die Musik zu schnell oder zu langsam abgespielt.
    if (rate_hz != 44100 && rate_hz != 48000) {
        return false;
    }
    if (rate_hz == s_sample_rate) {
        return true;                 // schon eingestellt, nichts zu tun
    }
    LockGuard guard(s_codec_mutex);
    kit.setAudioInfo(AudioInfo(rate_hz, 2, 16));
    s_sample_rate = rate_hz;
    return true;
}

size_t audio_output_write(const uint8_t *data, size_t len) {
    if (s_muted) {                   // nach audio_output_stop(): erste neue Daten schalten wieder ein
        LockGuard guard(s_codec_mutex);
        kit.setMute(false);
        s_muted = false;
    }
    return kit.write(data, len);
}

void audio_output_set_volume(uint8_t percent) {
    if (percent > 100) percent = 100;
    LockGuard guard(s_codec_mutex);
    kit.setVolume(percent / 100.0f); // die Bibliothek erwartet 0.0 bis 1.0
    s_volume_percent = percent;
}

uint8_t audio_output_get_volume(void) {
    return s_volume_percent;
}

void audio_output_stop(void) {
    // kit.end() wäre zu viel: das schaltet I2S und Codec komplett ab.
    LockGuard guard(s_codec_mutex);
    kit.setMute(true);
    s_muted = true;
}

void audio_output_test_tone(uint32_t duration_ms) {
    // Lokale Objekte sind hier in Ordnung: Sie werden nur benutzt, solange
    // diese Funktion läuft (anders als kit, das immer gebraucht wird).
    SineWaveGenerator<int16_t> sine(10000);          // Amplitude (max. 32767)
    GeneratedSoundStream<int16_t> sound(sine);
    AudioInfo info(s_sample_rate, 2, 16);
    sine.begin(info, 440.0f);                        // 440 Hz
    sound.begin(info);

    Serial.println("Audio output test tone...");
    uint8_t buffer[512];
    uint32_t start = millis();
    while (millis() - start < duration_ms) {
        size_t n = sound.readBytes(buffer, sizeof(buffer));
        audio_output_write(buffer, n);               // derselbe Weg wie später Bluetooth/Spotify
    }
    Serial.println("Audio output test tone done.");
}
