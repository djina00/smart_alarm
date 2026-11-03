#pragma once

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

// Pravi red događaja i pokreće task sa logikom alarma
void alarm_init(void);

// Šalje događaj logici alarma (poziva se iz drugih taskova)
void alarm_post_event(alarm_event_t event);

// Vraća trenutno stanje alarma
alarm_state_t alarm_get_state(void);
