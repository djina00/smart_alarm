#pragma once

#include "alarm.h"

// Pravi MQTT klijenta i pokreće task za komunikaciju sa serverom.
// Povezivanje na broker počinje kad uređaj dobije WiFi vezu.
void comm_init(void);

// Prijavljuje serveru detektovan pokret (dok je alarm uključen)
void comm_publish_motion(alarm_state_t state);

// Prijavljuje serveru novo stanje alarma
void comm_publish_state(alarm_state_t state);
