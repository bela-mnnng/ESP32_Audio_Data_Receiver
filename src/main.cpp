#include <Arduino.h>
#include "audio_output.h"
#include "bt_sink.h"
#include "wifi_manager.h"
#include "spotify_connect.h"
#include "mode_controller.h"

// Zustand ERROR aus docs/uml/uml.md: Fehler loggen, nach 5 s Neustart.
static void enter_error_state(const char *reason) {
    Serial.printf("ERROR: %s – Neustart in 5 s\n", reason);
    delay(5000);
    ESP.restart();
}

void setup() {
    Serial.begin(115200);
    if (!audio_output_init()) {                 // A
        enter_error_state("audio_output_init fehlgeschlagen");
    }
    bt_sink_init(mode_controller_post);         // A
    wifi_manager_init(mode_controller_post);    // B
    spotify_connect_init(mode_controller_post); // B
    mode_controller_init();                     // A, zuletzt
}

void loop() {
    mode_controller_update();
    delay(5);
}
