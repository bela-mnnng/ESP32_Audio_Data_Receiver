#ifndef BT_SINK_H
#define BT_SINK_H

#include "AudioTools.h"             // vor BluetoothA2DPSink.h, aktiviert die AudioTools-Ausgabe
#include "BluetoothA2DPSink.h"

extern bool bt_sink_init;
extern void bt_sink_start();

#endif // BT_SINK_H