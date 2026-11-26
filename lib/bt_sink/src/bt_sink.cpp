#include <Arduino.h>
#include "AudioTools.h"             // vor BluetoothA2DPSink.h, aktiviert die AudioTools-Ausgabe
#include "BluetoothA2DPSink.h"
#include "bt_sink.h"

// Genau EIN Sink-Objekt für das ganze Programm. "static" heißt: lebt so lange
// wie das Programm, ist aber nur in dieser Datei sichtbar.
static BluetoothA2DPSink a2dp_sink;

static app_event_cb_t s_event_cb = nullptr;      // hierüber Ereignisse an den mode_controller melden
static volatile uint32_t s_last_bt_data_ms = 0;  // Zeitpunkt des letzten Audiopakets

// Läuft im Bluetooth-Task: nur kurz etwas merken, kein Serial.print(), kein delay().
static void on_data(void) {
    s_last_bt_data_ms = millis();
}

void bt_sink_init(app_event_cb_t cb) {
    s_event_cb = cb;
    // Alle set_...-Callbacks müssen VOR a2dp_sink.start() gesetzt werden.
    a2dp_sink.set_on_data_received(on_data);
    // Bibliothek soll NICHT selbst auf I2S schreiben; I2S gehört allein audio_output.
    // TODO Schritt 2: statt nullptr eigenen Callback übergeben, der audio_output_write() aufruft.
    a2dp_sink.set_stream_reader(nullptr, false);
}

void bt_sink_start(void) {
    a2dp_sink.start(BT_DEVICE_NAME);
}

void bt_sink_stop(void) {
    a2dp_sink.end();                // ohne "true": ein späteres start() bleibt möglich
}
