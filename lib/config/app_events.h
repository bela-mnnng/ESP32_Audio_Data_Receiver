#pragma once
/*
 * app_events.h – gemeinsame Ereignisse zwischen allen Komponenten
 *
 * Owner: gemeinsam (Person A + Person B)
 * Neue Einträge nur per gemeinsamem Pull Request.
 * Beschreibung: docs/schnittstellen.md
 */

typedef enum {
    /* --- Bluetooth (Person A, gemeldet von bt_sink) --- */
    EVT_BT_CONNECTED = 0,          /* Smartphone per A2DP verbunden */
    EVT_BT_DISCONNECTED,           /* Bluetooth-Verbindung getrennt */

    /* --- WLAN (Person B, gemeldet von wifi_manager) --- */
    EVT_WIFI_CONNECTED,            /* IP-Adresse erhalten */
    EVT_WIFI_CONNECT_FAILED,       /* Timeout oder keine Zugangsdaten */
    EVT_WIFI_LOST,                 /* bestehende Verbindung verloren */

    /* --- Spotify Connect (Person B, gemeldet von spotify_connect) --- */
    EVT_SPOTIFY_PLAYBACK_STARTED,  /* Adapter in der App gewählt, Audio läuft */
    EVT_SPOTIFY_PLAYBACK_STOPPED,  /* anderes Gerät gewählt oder Sitzung beendet */

    /* --- intern im mode_controller (Person A) --- */
    EVT_RETRY_TIMER,               /* Wartezeit in IDLE abgelaufen */

    EVT_COUNT
} app_event_t;

/*
 * Callback, über den jede Komponente Ereignisse meldet.
 * Darf aus jedem Task aufgerufen werden und blockiert nicht.
 * In main.cpp wird mode_controller_post() übergeben.
 */
typedef void (*app_event_cb_t)(app_event_t event);
