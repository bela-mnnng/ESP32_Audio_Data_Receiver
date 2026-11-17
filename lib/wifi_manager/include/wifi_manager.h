#pragma once
/*
 * wifi_manager.h – Verbindung zum Heim-WLAN
 *
 * Owner: Person B
 * Gesteuert von: mode_controller (Person A)
 * Beschreibung: docs/schnittstellen.md
 */
#include <stdbool.h>
#include "app_events.h"

#define WIFI_CONNECT_TIMEOUT_MS 15000

/*
 * Einmal in setup(). Über cb kommen:
 *   EVT_WIFI_CONNECTED, EVT_WIFI_CONNECT_FAILED, EVT_WIFI_LOST
 */
void wifi_manager_init(app_event_cb_t cb);

/*
 * Verbindungsversuch mit den Zugangsdaten aus secrets.h starten.
 * Kehrt sofort zurück. Ergebnis per Callback: EVT_WIFI_CONNECTED oder
 * spätestens nach WIFI_CONNECT_TIMEOUT_MS EVT_WIFI_CONNECT_FAILED.
 * Verbindet nach einem Verlust NICHT selbst neu; das entscheidet der mode_controller.
 */
void wifi_manager_start(void);

/*
 * WLAN vollständig ausschalten (Funk aus), auch mitten im Verbindungsversuch.
 * Kehrt erst zurück, wenn WLAN aus ist (höchstens 500 ms).
 * Danach meldet wifi_manager keine Ereignisse mehr bis zum nächsten start().
 */
void wifi_manager_stop(void);

bool wifi_manager_is_connected(void);
