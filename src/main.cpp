#include <Arduino.h>
#include "audio_output.h"
#include "bt_sink.h"
#include "wifi_manager.h"
#include "spotify_connect.h"
#include "mode_controller.h"

void setup() {
    Serial.begin(115200);
    audio_output_init();                        // A
    bt_sink_init(mode_controller_post);         // A
    wifi_manager_init(mode_controller_post);    // B
    spotify_connect_init(mode_controller_post); // B
    mode_controller_init();                     // A, zuletzt
}

void loop() {
    mode_controller_update();
    delay(5);
}
