#pragma once

#include "esp_err.h"

// Događaji koje prima logika alarma
typedef enum {
    ALARM_EVENT_MOTION,      // PIR je detektovao pokret
    ALARM_EVENT_TOGGLE_ARM,  // dugme: uključi ako je isključen, inače isključi
    ALARM_EVENT_ARM,         // komanda sa servera: uključi alarm
    ALARM_EVENT_DISARM,      // komanda sa servera: isključi alarm
} alarm_event_t;

// Stanja alarma
typedef enum {
    ALARM_STATE_DISARMED,    // isključen: pokret se ignoriše
    ALARM_STATE_ARMED,       // uključen: čeka pokret
    ALARM_STATE_TRIGGERED,   // alarm: sirena radi
} alarm_state_t;

// Poziva se kad alarm pređe u novo stanje
typedef void (*alarm_state_cb_t)(alarm_state_t new_state);

// Poziva se kad je pokret detektovan dok je alarm uključen
typedef void (*alarm_motion_cb_t)(alarm_state_t state);

// Pravi red događaja i pokreće task sa logikom alarma.
// Callback funkcije se pozivaju iz taska alarma i moraju biti kratke.
esp_err_t alarm_init(alarm_state_cb_t on_state_change, alarm_motion_cb_t on_armed_motion);

// Šalje događaj logici alarma (poziva se iz drugih taskova)
void alarm_post_event(alarm_event_t event);

// Vraća trenutno stanje alarma
alarm_state_t alarm_get_state(void);

// Kod stanja za slanje na server: "DISARMED", "ARMED" ili "TRIGGERED"
const char *alarm_state_code(alarm_state_t state);
