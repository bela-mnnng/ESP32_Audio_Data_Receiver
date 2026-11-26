#pragma once
/*
 * bt_sink.h – Bluetooth-A2DP-Empfänger (Smartphone → Adapter)
 *
 * Owner: Person A
 * Gesteuert von: mode_controller
 * Gibt Audio ausschließlich über audio_output_write() aus.
 * Beschreibung: docs/interfaces.md
 *
 * Die Bibliotheks-Header (AudioTools.h, BluetoothA2DPSink.h) stehen bewusst
 * nur in bt_sink.cpp. Wer diesen Header einbindet, braucht sie nicht.
 */
#include <stdbool.h>
#include "app_events.h"

#define BT_DEVICE_NAME "Bluetooth Audio Adapter"

/*
 * Einmal in setup(). Über cb kommen:
 *   EVT_BT_CONNECTED, EVT_BT_DISCONNECTED
 */
void bt_sink_init(app_event_cb_t cb);

/* Bluetooth einschalten und unter BT_DEVICE_NAME sichtbar machen. */
void bt_sink_start(void);

/* Bluetooth ausschalten. Ein späteres bt_sink_start() ist weiterhin möglich. */
void bt_sink_stop(void);
