#include "mode_controller.h"
#include "bt_sink.h"
#include "wifi_manager.h"
#include "spotify_connect.h"
#include "audio_output.h"

static app_mode_t s_mode = MODE_BOOT;

// Wird nur aufgerufen, wenn alle *_init() in setup() geklappt haben
// (Fehlerfall: siehe enter_error_state() in main.cpp).
void mode_controller_init(void) {
    bt_sink_start();
    wifi_manager_start();
    s_mode = MODE_WLAN_VERBINDEN;
}

void mode_controller_post(app_event_t event) {
    // TODO Schritt 4: Ereignis in eine FreeRTOS-Queue legen (docs/api_reference.md Abschnitt 7)
    (void)event;
}

void mode_controller_update(void) {
    // TODO Schritt 4: Queue leeren, mode_fsm fragen, start()/stop() der Komponenten aufrufen
}

app_mode_t mode_controller_get_mode(void) {
    return s_mode;
}
