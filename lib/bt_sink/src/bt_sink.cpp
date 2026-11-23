#include "bt_sink.h"

volatile uint32_t last_bt_data_ms = 0;


void bt_sink_init() {
    BluetoothA2DPSink a2dp_sink;                    // Erstelle eine Instanz des Bluetooth A2DP Sinks
}

void bt_sink_start() {
    BluetoothA2DPSink a2dp_sink;
    a2dp_sink.start("Bluetooth Audio Adapter");     // Starte den Bluetooth A2DP Sink mit dem angegebenen Namen
}

void bt_sink_end()  {
    BluetoothA2DPSink a2dp_sink;
    a2dp_sink.end();                                // Beende den Bluetooth A2DP Sink (Erneute Verbindung möglich)
}

void bt_sink_run() {
    BluetoothA2DPSink a2dp_sink;
    a2dp_sink.set_on_data_received(on_data);
    printf("Last BT data received at: %lu ms\n", last_bt_data_ms);
}

void on_data(){ 
    last_bt_data_ms = millis(); 
}

