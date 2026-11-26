#pragma once
/*
 * mode_controller.h – Zustandsautomat für die Betriebsmodi
 *
 * Owner: Person A (Review durch Person B)
 * Die Komponenten von Person B binden diesen Header NICHT ein.
 * Sie melden Ereignisse über den app_event_cb_t, den main.cpp verdrahtet.
 * Beschreibung: docs/interfaces.md, Zustände: docs/uml/uml.md
 */
#include "app_events.h"
#include "mode_fsm.h"       // app_mode_t

/* Nach allen anderen *_init()-Aufrufen in setup(). Startet WLAN und Bluetooth. */
void mode_controller_init(void);

/*
 * Ereignis in die Queue legen. Passt auf app_event_cb_t.
 * Thread-sicher, blockiert nicht, aus jedem Task aufrufbar.
 */
void mode_controller_post(app_event_t event);

/* Aus loop() aufrufen. Verarbeitet die Queue und ruft start()/stop() der Komponenten. */
void mode_controller_update(void);

app_mode_t mode_controller_get_mode(void);
