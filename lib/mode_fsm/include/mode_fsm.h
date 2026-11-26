#pragma once
/*
 * mode_fsm.h – reine Logik des Zustandsautomaten
 *
 * Owner: Person A
 * Kein Arduino.h und keine anderen Komponenten einbinden, damit die
 * Unit-Tests am PC laufen (pio test -e native).
 * Zustände: docs/uml/uml.md, Übergänge: docs/interfaces.md Abschnitt 4
 */
#include "app_events.h"

typedef enum {
    MODE_BOOT = 0,
    MODE_ERROR,
    MODE_WLAN_VERBINDEN,
    MODE_WLAN_BEREIT,
    MODE_WLAN_STREAMING,
    MODE_IDLE,
    MODE_BT_STREAMING
} app_mode_t;
