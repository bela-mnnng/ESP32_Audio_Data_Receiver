#pragma once
/*
 * spotify_connect.h – Adapter als Spotify-Connect-Gerät (über cspot)
 *
 * Owner: Person B
 * Gesteuert von: mode_controller (Person A)
 * Gibt Audio ausschließlich über audio_output_write() aus.
 * Beschreibung: docs/schnittstellen.md
 */
#include <stdbool.h>
#include "app_events.h"

#define SPOTIFY_DEVICE_NAME "ESP32 Audio Adapter"

/*
 * Einmal in setup(). Über cb kommen:
 *   EVT_SPOTIFY_PLAYBACK_STARTED, EVT_SPOTIFY_PLAYBACK_STOPPED
 * Pause in der App erzeugt KEIN Ereignis; der Adapter bleibt aktives Gerät.
 */
void spotify_connect_init(app_event_cb_t cb);

/*
 * Adapter in der Spotify-App sichtbar machen. Nur aufrufen, wenn das
 * WLAN verbunden ist. Kehrt sofort zurück.
 */
void spotify_connect_start(void);

/*
 * Wiedergabe beenden und Gerät abmelden. Kehrt erst zurück, wenn
 * garantiert keine audio_output_write()-Aufrufe mehr folgen (höchstens 1 s).
 * Danach meldet spotify_connect keine Ereignisse mehr bis zum nächsten start().
 */
void spotify_connect_stop(void);

bool spotify_connect_is_playing(void);
