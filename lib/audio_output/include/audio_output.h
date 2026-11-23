#pragma once
/*
 * audio_output.h – Audioausgabe über I2S und Codec ES8388
 *
 * Owner: Person A
 * Genutzt von: bt_sink (Person A), spotify_connect (Person B)
 * Beschreibung: docs/schnittstellen.md
 *
 * PCM-Format für audio_output_write():
 *   16 Bit signed, little endian, Stereo, verschachtelt (L, R, L, R, ...)
 *
 * Es schreibt immer nur EINE Quelle gleichzeitig. Das stellt der
 * mode_controller sicher, indem er die alte Quelle stoppt, bevor die
 * neue freigegeben wird.
 */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <AudioTools.h>

#define AUDIO_OUTPUT_DEFAULT_RATE_HZ 44100

/* Codec und I2S starten. Einmal in setup(). true = Erfolg. */
bool audio_output_init(void);

/*
 * Abtastrate setzen. Jede Quelle ruft das auf, bevor sie die ersten
 * Daten schreibt. Erlaubt: 44100 oder 48000. false = nicht unterstützt.
 */
bool audio_output_set_sample_rate(uint32_t rate_hz);

/*
 * PCM-Daten ausgeben. Blockiert, bis die Daten im I2S-Puffer liegen
 * (höchstens ca. 50 ms). Darf aus dem Audio-Task der Quelle aufgerufen werden.
 * Rückgabe: Anzahl geschriebener Bytes.
 */
size_t audio_output_write(const uint8_t *data, size_t len);

/* Lautstärke in Prozent (0–100). Thread-sicher. */
void    audio_output_set_volume(uint8_t percent);
uint8_t audio_output_get_volume(void);

/* Puffer leeren und stumm schalten, z. B. beim Quellenwechsel. */
void audio_output_stop(void);

